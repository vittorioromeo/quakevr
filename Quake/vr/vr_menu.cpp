// vr_menu.cpp -- the "VR Settings" pages (Options > VR Settings), drawn like Ironwail's options
// pages: scrolling lists of labelled settings, changed with left/right (the sticks in VR), with
// actions on enter (A). "Advanced VR Options" at the bottom opens a list of further pages: the old
// Quake VR settings pages (vr_menu_pages.inc) and the new body, throwing and force grab tweaks,
// grouped by topic, the long ones split into pages of a screenful or so. In a headset the pages are
// taller (vr_menu_height, vr_menuui.cpp): more rows at once. Escape (B) goes back a page. With the mouse (and the VR laser pointer, vr_menuui.cpp): the row under
// it is selected, a click picks it, and a slider is set where it is clicked and dragged.

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_weapons.hpp"
#include "vr_hands.hpp"
#include "vr_view.hpp"

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
        Action
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

    // Shown under the list while selected.
    const char* helpText{nullptr};

    // Slider: shown instead of the value while it is negative (-1: a hue following the player's).
    const char* negativeLabel{nullptr};

    [[nodiscard]] Item help(const char* text) const
    {
        Item i = *this;
        i.helpText = text;
        return i;
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

[[nodiscard]] Item open(const char* label, int page)
{
    Item i{Item::Action, label};
    i.page = page;
    return i;
}

using PageBuilder = std::vector<Item> (*)();

// The index of the page `build` builds (pages[], below): what a link to it opens.
[[nodiscard]] int pageIndex(PageBuilder build);

#include "vr_menu_pages.inc"

// ----------------------------------------------------------------------------
// Pages of the port's own tweaks
// ----------------------------------------------------------------------------

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
        slider("Damage to Enemies", vr_damage_to_enemies, 0.25f, 4.f, 0.05f, "%.2fx").help("Damage you deal to monsters."),
        slider("Damage to You", vr_damage_to_player, 0.f, 4.f, 0.05f, "%.2fx").help("Damage monsters, traps and falls deal to you."),
        slider("Self Damage", vr_damage_self, 0.f, 2.f, 0.05f, "%.2fx").help("Damage your own rockets and grenades deal to you."),
        slider("Melee Damage", vr_melee_dmg_multiplier, 0.25f, 15.f, 0.25f, "%.2fx"),
        header("Positional Damage"),
        toggle("Positional Damage", vr_positional_damage).help("Headshots, arm and leg shots on humanoid monsters."),
        slider("Headshot Damage", vr_headshot_mult, 1.f, 5.f, 0.1f, "%.1fx"),
        slider("Arm Shot Damage", vr_limbshot_mult, 0.1f, 1.f, 0.05f, "%.2fx"),
        slider("Leg Shot Damage", vr_legshot_mult, 0.1f, 1.f, 0.05f, "%.2fx"),
        slider("Headshot Sound", vr_headshot_sound, 0.f, 1.f, 0.1f, "%.1f").help("Volume of the crack you hear when you land a headshot (0 off)."),
        header("Knockback"),
        slider("Knockback", vr_push, 0.f, 2.f, 0.05f, "%.2fx").help("All knockback; the settings below scale this."),
        slider("Your Melee Hits", vr_melee_push, 0.f, 3.f, 0.05f, "%.2fx").help("How far your melee blows (and headbutts) push what they hit."),
        slider("Weapon Hits", vr_hit_push, 0.f, 3.f, 0.05f, "%.2fx").help("How far heavy weapon hits shove monsters."),
        slider("Killing Blows", vr_kill_push, 0.f, 3.f, 0.05f, "%.2fx").help("How far killing blows and explosions throw the bodies."),
        slider("Parry Pushes Enemy", vr_parry_push_enemy, 0.f, 3.f, 0.05f, "%.2fx"),
        slider("Parry Pushes You", vr_parry_push_player, 0.f, 3.f, 0.05f, "%.2fx"),
        slider("Monsters' Blows Push You", vr_melee_push_player, 0.f, 3.f, 0.05f, "%.2fx"),
        header("Knights' Swords"),
        slider("Knights Drop Swords", vr_sword_drop, 0.f, 1.f, 0.05f, "%.2f").help("Chance a dying knight or hell knight drops its sword, a melee weapon you can pick up."),
        slider("Sword Damage", vr_sword_damage_mult, 0.5f, 3.f, 0.05f, "%.2fx").help("A sword swing's damage over the axe's (the hell knight's sword: 25% more)."),
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
        slider("Parry Angle", vr_parry_angle, 15.f, 80.f, 5.f, "%.0f deg")
            .help("Hold a weapon (sword, axe or gun, one hand or two) level across in front of you to block a monster's melee blow: how far it may be tilted off level."),
        slider("Parry Reach", vr_parry_reach, 0.5f, 2.5f, 0.1f, "%.1f m").help("How far in front of you a held weapon still parries."),
        toggle("Unarmed Parry", vr_parry_unarmed).help("Cross your arms in an X in front of you to block a blow with your forearms."),
        slider("Unarmed Parry Reduction", vr_parry_unarmed_reduction, 0.f, 1.f, 0.05f, "%.2f"),
        toggle("Bash", vr_bash).help("Hold a guard (a weapon level across in front, as for a parry, one hand or two) still, then push it forward: knocks monsters back and staggers them. Open palms pushed at a monster shove it (one hand: half as hard)."),
        slider("Bash Speed", vr_bash_speed, 0.8f, 3.f, 0.1f, "%.1f m/s").help("How fast the guard must be pushed forward (less than a blow needs)."),
        slider("Bash Guard Hold", vr_bash_hold, 0.f, 0.6f, 0.05f, "%.2f s")
            .help("How long the guard must be held still before the push. A swing whose blade passes through level on its way never holds it."),
        slider("Bash Swing Limit", vr_bash_swing_rate, 30.f, 400.f, 10.f, "%.0f deg/s")
            .help("A weapon turning faster than this is a swing, not a bash (held still, the guard turns under half of it). Lower it if swings still bash; raise it if your pushes tilt the blade and don't bash."),
        slider("Shove Speed", vr_shove_speed, 0.8f, 4.f, 0.1f, "%.1f m/s")
            .help("How fast open palms (facing ahead, not holding anything) must be pushed forward to shove. Punches, slaps, slow reaches and a hand on a sword's grip never shove."),
        slider("Bash Damage", vr_bash_damage, 0.f, 40.f, 1.f, "%.0f"),
        slider("Bash Push", vr_bash_push, 0.f, 3.f, 0.05f, "%.2fx").help("How far a bash or shove throws what it hits (times Knockback)."),
        slider("Bash and Parry Sounds", vr_bash_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of the sounds that tell a shove, a weapon bash, a parry-bash (a bash right after a parry) and a parry apart from your blows (0: the old sounds)."),
        header("Sword: Blade, Hilt, Stab"),
        slider("Hilt Waits for Blade", vr_melee_hilt_window, 0.05f, 0.5f, 0.05f, "%.2f s")
            .help("In a swing the hands often reach a monster before the blade: the hilt's touch waits this long for the blade to sweep in, and a swing never lands with the hilt. A pommel strike (the pommel leading) still does."),
        slider("Stab Speed", vr_melee_stab_speed, 0.3f, 1.5f, 0.05f, "%.2fx")
            .help("A stab (the sword driven along its blade, the tip first) pierces: its least speed, times Swing Speed (a swing needs 1.25x)."),
        header("Batting Projectiles"),
        toggle("Bat Back Projectiles", vr_deflect).help("Swing a weapon (or a fist) through a monster's spike, laser, spit or grenade to send it back where your hand points (at the monster, when you point near it)."),
        slider("Batting Reach", vr_deflect_radius, 4.f, 32.f, 1.f, "%.0f units").help("How near the weapon's blade (or your fist) a projectile must pass to be batted back."),
        slider("Batting Swing Speed", vr_deflect_speed, 0.2f, 1.5f, 0.05f, "%.2fx").help("How fast a batting swing must be, times Swing Speed (a hit needs 1x, and more for a swung weapon)."),
        slider("Batting Timing", vr_deflect_window, 0.f, 0.5f, 0.05f, "%.2f s").help("How early you may swing: the weapon's path keeps batting this long after it passed."),
        slider("Bash Batting Reach", vr_bash_deflect_radius, 4.f, 48.f, 1.f, "%.0f units")
            .help("A bash (or a shove with a weapon in hand) bats back projectiles that pass this near the guard: the weapon and the hands."),
        slider("Bash Batting Timing", vr_bash_deflect_window, 0.f, 1.f, 0.05f, "%.2f s").help("How long a bash goes on batting after the push."),
        header("Headbutt"),
        toggle("Headbutt", vr_headbutt).help("Lunge your head at something to headbutt it."),
        slider("Headbutt Speed", vr_headbutt_speed, 0.4f, 3.f, 0.05f, "%.2f m/s").help("How fast the head must lunge (towards where you look)."),
        slider("Headbutt Damage", vr_headbutt_damage, 5.f, 100.f, 1.f, "%.0f"),
    };
}

[[nodiscard]] std::vector<Item> pageBody()
{
    return {
        cycle("Body", vr_body_mode, {{0.f, "Off"}, {2.f, "Torso and arms"}, {3.f, "Full body"}}),
        cycle("Build", vr_body_build, {{0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}}),
        toggle("Walking Legs", vr_body_walk).help("The legs (full body) walk as you move with the stick."),
        slider("Step Rate", vr_body_step_rate, 1.f, 5.f, 0.1f, "%.1f /s")
            .help("How fast the legs step at most, in steps a second at full running speed (walking, somewhat fewer)."),
        slider("Turn Before Stepping", vr_body_turn_step, 15.f, 90.f, 5.f, "%.0f deg")
            .help("How far you turn over your planted feet before they step round to follow."),
        slider("Wading Heaviness", vr_body_wade, 0.f, 2.f, 0.1f, "%.1fx")
            .help("Wading, the legs walk heavier: shorter, higher, slower steps (0: as on land)."),
        slider("Swimming Kicks", vr_body_swim_kick, 0.f, 2.f, 0.1f, "%.1fx")
            .help("Swimming, the legs trail behind and kick where the stick moves you: how wide (0: no kicks)."),
        slider("Swimming Kick Rate", vr_body_swim_kick_rate, 0.f, 4.f, 0.1f, "+%.1f /s")
            .help("How many more kicks a second at full stick (treading water, about 0.7)."),
        toggle("Show Armour and Wounds", vr_body_state)
            .help("The armour you wear plates your torso; your arms and hands get bloodier as you are hurt."),
        toggle("Wounds Drip Blood", vr_body_blood)
            .help("Blood drips from your wounded arms and hands, faster when badly hurt or just hit, and splashes on the floor."),
        toggle("Show Powerups", vr_body_powerups)
            .help("Quad damage sparks around your hands, the pentagram makes you glow, the ring fades you."),
        toggle("Anchors Follow Body", vr_body_anchors)
            .help("Holsters, the virtual stock and hand collisions follow the body's lean and crouch."),
        slider("Hip Holsters Follow Legs", vr_holster_leg_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("Full body: the hip holsters move with the walking and kicking legs (0: fixed on the body, 1: all the way)."),
        header("Placement"),
        slider("Torso Offset", vr_body_torso_back, -0.15f, 0.3f, 0.01f, "%.2f m")
            .help("How far the torso sits behind your neck (negative: in front)."),
        slider("Legs Offset", vr_body_legs_back, -0.15f, 0.3f, 0.01f, "%.2f m")
            .help("How far the feet stand behind your head (negative: in front)."),
        slider("Shoulders Back", vr_body_shoulders_back, -0.1f, 0.15f, 0.01f, "%.2f m")
            .help("How far the shoulders sit behind the chest (negative: in front)."),
        slider("Shoulders Up", vr_body_shoulders_up, -0.1f, 0.1f, 0.01f, "%.2f m"),
        slider("Shoulders Width", vr_body_shoulders_out, -0.08f, 0.08f, 0.01f, "%.2f m")
            .help("How much further out each shoulder is (negative: in)."),
        slider("Eyes Forward", vr_body_eye_forward, 0.f, 0.2f, 0.01f, "%.2f m")
            .help("From the top of the neck to the eyes, forward."),
        slider("Eyes Up", vr_body_eye_up, 0.f, 0.2f, 0.01f, "%.2f m").help("From the top of the neck to the eyes, up."),
        slider("Crouch Tilt", vr_body_crouch_tilt, 0.f, 60.f, 5.f, "%.0f deg")
            .help("How far the back tilts forward in a full crouch (the hips stay under you)."),
    };
}

// Split from Body: the arms' reach and bend, and the pauldrons over the shoulders.
[[nodiscard]] std::vector<Item> pageBodyArms()
{
    return {
        slider("Arm Length", vr_body_arm_length, 0.8f, 1.3f, 0.01f, "%.2f"),
        slider("Arm Stretch", vr_body_arm_stretch, 1.f, 1.5f, 0.05f, "%.2f")
            .help("How far arms may stretch to reach the hands (1: not at all)."),
        slider("Shoulder Reach", vr_body_shoulder_reach, 0.f, 0.2f, 0.01f, "%.2f m")
            .help("How far the shoulders may move out beyond that."),
        slider("Forearm Twist", vr_body_forearm_twist, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of the wrist's roll the forearm follows."),
        slider("Elbow Out", vr_body_elbow_out, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Elbow Back", vr_body_elbow_back, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Elbow From Hand", vr_body_elbow_hand, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the elbow points away from the back of the hand."),
        slider("Shoulders Up", vr_body_shoulder_up, 0.f, 45.f, 1.f, "%.0f deg")
            .help("How far the shoulders rise when reaching up."),
        slider("Shoulders Forward", vr_body_shoulder_forward, 0.f, 45.f, 1.f, "%.0f deg")
            .help("How far the shoulders swing forward when reaching far forward."),
        header("Pauldrons"),
        toggle("Pauldrons", vr_body_pauldrons).help("Leather pads over the shoulders and the tops of the arms, as the Quake ranger wears."),
        cycle("Pauldron Style", vr_body_pauldron_style, {{0.f, "Ranger leather"}, {1.f, "Armour colour"}, {2.f, "Steel"}})
            .help("Armour colour: green, yellow or red as the armour you wear (leather without)."),
        slider("Pauldron Size", vr_body_pauldron_size, 0.5f, 1.5f, 0.05f, "%.2fx"),
        slider("Pauldron Follows Arm", vr_body_pauldron_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the shoulder cap turns with the upper arm (the lower plates follow the arm fully)."),
        slider("Pauldron Forward", vr_body_pauldron_forward, -0.05f, 0.05f, 0.005f, "%.3f m"),
        slider("Pauldron Up", vr_body_pauldron_up, -0.05f, 0.05f, 0.005f, "%.3f m"),
        slider("Pauldron Out", vr_body_pauldron_out, -0.05f, 0.05f, 0.005f, "%.3f m"),
    };
}

// Split from Body: the torch on the chest.
[[nodiscard]] std::vector<Item> pageFlashlight()
{
    return {
        toggle("Chest Flashlight", vr_flashlight)
            .help("A torch on your chest. Trigger at it: on or off. Grip it with an empty hand to take it; let go and it springs back."),
        slider("Brightness", vr_flashlight_brightness, 0.25f, 2.5f, 0.05f, "%.2fx"),
        slider("Range", vr_flashlight_range, 300.f, 2000.f, 50.f, "%.0f"),
        slider("Visible Beam", vr_flashlight_beam, 0.f, 1.f, 0.05f, "%.2f").help("A soft cone of light in the air from the lamp (0: none)."),
        toggle("Casts Shadows", vr_flashlight_shadows).help("Its light casts shadows (takes one of the shadowed dynamic lights)."),
        slider("Tilt Down", vr_flashlight_tilt, -10.f, 30.f, 1.f, "%.0f deg").help("How far below where your torso faces the clipped lamp points."),
        slider("Forward", vr_flashlight_forward, -0.05f, 0.05f, 0.005f, "%.3f m"),
        slider("Up", vr_flashlight_up, -0.15f, 0.15f, 0.01f, "%.2f m"),
        slider("Out", vr_flashlight_out, -0.08f, 0.08f, 0.01f, "%.2f m").help("Towards your off hand's side."),
        slider("In Hand Forward", vr_flashlight_hand_forward, -0.1f, 0.05f, 0.005f, "%.3f m").help("Where the held lamp sits in your fist."),
        slider("In Hand Up", vr_flashlight_hand_up, -0.1f, 0.05f, 0.005f, "%.3f m"),
        slider("On Gun Forward", vr_flashlight_gun_forward, -0.15f, 0.05f, 0.005f, "%.3f m")
            .help("Held near the gun in your other hand, B or Y clips it under the barrel. B or Y at it takes it off."),
        slider("On Gun Up", vr_flashlight_gun_up, -0.05f, 0.05f, 0.005f, "%.3f m"),
        slider("On Gun Out", vr_flashlight_gun_out, -0.05f, 0.05f, 0.005f, "%.3f m").help("Away from your body."),
    };
}

// Gore (vr_gore.cpp, vr_decals.cpp, vr_bodyblood.cpp; the QC's gibs sticking: vr_carry.qc).
[[nodiscard]] std::vector<Item> pageGore()
{
    return {
        cycle("Gore", vr_gore, {{0.f, "Quake VR"}, {1.f, "More"}, {2.f, "Over the top"}})
            .help("Over the top: hits spray blood onto the walls, floor and ceiling behind, gibbing paints the room, pools spread under corpses, gibs stick to ceilings and drip."),
        header("Hits, Gibs and Corpses"),
        slider("Blood Sprays", vr_gore_spray, 0.f, 3.f, 0.25f, "%.2fx")
            .help("How many splats a hit or a gibbing throws onto the walls, floor and ceiling round it (0 none)."),
        slider("Splat Size", vr_gore_size, 0.5f, 2.f, 0.1f, "%.1fx").help("How big the gore's splats, pools and runs are."),
        slider("Blood Pools", vr_gore_pools, 0.f, 2.f, 0.1f, "%.1fx").help("Pools of blood spreading under corpses and gibs: their size (0 none)."),
        slider("Dripping", vr_gore_drips, 0.f, 3.f, 0.25f, "%.2fx")
            .help("Blood dripping from splats on the ceiling and from gibs stuck there: how long and how much (0 none)."),
        slider("Gibs Stick", vr_gore_stick, 0.f, 30.f, 1.f, "%.0f s")
            .help("Gibs flung into a ceiling or a wall may stick there about this long, dripping, then fall (0 never)."),
        header("Your Wounds"),
        slider("Arm Drip Rate", vr_body_blood, 0.f, 4.f, 0.25f, "%.2fx")
            .help("How often blood drips from your wounded arms and hands (the body's wounds: Show Armour and Wounds; 0 none)."),
        slider("Drop Size", vr_body_blood_amount, 0.5f, 3.f, 0.25f, "%.2fx").help("How big the drops are and how much they splash."),
        slider("Drips Round Feet", vr_body_blood_floor, 0.f, 4.f, 0.25f, "%.2fx")
            .help("While wounded, blood drips from your body round your feet, faster when badly hurt or just hit (0 none)."),
        slider("Drops Mark Floor", vr_body_blood_marks, 0.f, 1.f, 0.05f, "%.2f").help("The chance a drop leaves a mark on the floor."),
        slider("Floor Mark Size", vr_body_blood_mark_size, 0.5f, 4.f, 0.25f, "%.2fx"),
        header("Marks"),
        toggle("Decals", vr_decals).help("Blood, scorch marks and bullet chips on walls and floors (the gore needs them)."),
        slider("Max Decals", vr_decal_max, 64.f, 4096.f, 64.f, "%.0f").help("The oldest go first. The gore makes many: 1024 or more."),
        slider("Decal Lifetime", vr_decal_life, 10.f, 600.f, 10.f, "%.0f s"),
        toggle("Gib Blood", vr_gib_blood).help("Gibs and heads leave a trail of blood drops and splat where they hit walls and floors. Off: Quake's trail."),
        slider("Gib Blood Trail", vr_gib_blood_trail, 0.f, 3.f, 0.25f, "%.2fx").help("How dense their trail of blood and drops is (0 none)."),
    };
}

[[nodiscard]] std::vector<Item> pageGadget()
{
    return {
        cycle("HUD", vr_hud_mode, {{1.f, "Wrist gadget"}, {0.f, "Status bar"}}),
        cycle("Arm", vr_gadget_hand, {{0.f, "Off hand"}, {1.f, "Main hand"}}),
        slider("Size", vr_gadget_scale, 0.5f, 2.f, 0.05f, "%.2fx"),
        header("Placement"),
        slider("Along the Arm", vr_gadget_x, -10.f, 10.f, 0.5f, "%.1f cm"),
        slider("Across the Arm", vr_gadget_y, -5.f, 5.f, 0.25f, "%.2f cm"),
        slider("Height", vr_gadget_z, -3.f, 5.f, 0.25f, "%.2f cm").help("How far it stands out of the forearm."),
        slider("Pitch", vr_gadget_pitch, -90.f, 90.f, 5.f, "%.0f deg"),
        slider("Yaw", vr_gadget_yaw, -90.f, 90.f, 5.f, "%.0f deg"),
        slider("Roll", vr_gadget_roll, -180.f, 180.f, 15.f, "%.0f deg")
            .help("Turns the screen: 90 reads along the arm, 180 turns the text the other way."),
    };
}

// Split from Wrist Gadget (and Immersion): the wrist gadget's screen, the weapons' ammo screens and
// the maps' text boards.
[[nodiscard]] std::vector<Item> pageScreens()
{
    return {
        header("Wrist Gadget"),
        toggle("Level and Stats", vr_gadget_show_level),
        slider("Screen Light", vr_gadget_light, 0.f, 3.f, 0.1f, "%.1fx")
            .help("The screen casts a light in its colour the way it faces, and a faint one on your hand (0 off)."),
        slider("CRT Look", vr_gadget_crt, 0.f, 2.f, 0.1f, "%.1fx")
            .help("Scanlines, a slight flicker, faint static and now and then a glitch (0 off)."),
        slider("Screen Glow", vr_screen_glow, 0.f, 3.f, 0.1f, "%.1fx")
            .help("The gadget's and your weapons' screens glow softly round their edges (0 off)."),
        slider("Text Glow", vr_screen_text_glow, 0.f, 3.f, 0.1f, "%.1fx")
            .help("The text, numbers and icons on those screens glow: bright whitish cores, a soft halo (0 off)."),
        header("Messages"),
        toggle("Game Messages as Hologram", vr_messages_hologram)
            .help("The game's messages (a key needed, a secret found, the map's text, pickups) float as a hologram the gadget projects over its screen, while you look at it. Else the key and map messages show in front of you."),
        slider("Hologram Time", vr_messages_hologram_time, 2.f, 15.f, 0.5f, "%.1f s").help("How long a message stays in the hologram."),
        slider("Hologram Text Size", vr_messages_hologram_size, 0.5f, 2.f, 0.05f, "%.2fx"),
        slider("Hologram Height", vr_messages_hologram_height, 0.f, 10.f, 0.5f, "%.1f cm")
            .help("How high over the gadget it floats."),
        slider("Hologram Effect", vr_messages_hologram_effect, 0.f, 2.f, 0.1f, "%.1fx")
            .help("The beam of light from the screen, scanlines, flicker, glitches and the projection as it appears (0: plain glowing text)."),
        cycle("Console Messages", vr_notify_wrist, {{1.f, "Over the gadget"}, {2.f, "Both"}, {0.f, "In view"}})
            .help("The console's other messages (the engine's: settings changed, cheats, errors) float in a small log over the gadget, or at the top of the view."),
        slider("Console Message Time", vr_notify_wrist_time, 2.f, 30.f, 1.f, "%.0f s")
            .help("How long a message stays in the gadget's log."),
        slider("Console Log Height", vr_notify_wrist_height, 0.f, 20.f, 0.5f, "%.1f cm")
            .help("How high over the gadget the log floats (always over the hologram)."),
        slider("Console Log Brightness", vr_notify_wrist_alpha, 0.2f, 1.f, 0.05f, "%.2f"),
        header("Weapons' Ammo Screens"),
        toggle("Weapon Text", "vr_show_weapon_text").help("Show floating ammunition text attached to weapons"),
        toggle("Weapon Ammo Screen", "vr_weapon_screen").help("The ammunition text on a small screen on the weapon (colours from the wrist gadget's screen)."),
        slider("Ammo Screen Margin", "vr_weapon_screen_padding", 0.f, 2.f, 0.1f, "%.1f"),
        slider("Ammo Screen CRT Look", "vr_weapon_screen_crt", 0.f, 2.f, 0.1f, "%.1fx").help("Scanlines, a slight flicker, faint static and now and then a glitch, as on the wrist gadget's screen (0 off)."),
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
        slider("Screen Brightness", vr_gadget_screen_brightness, 0.3f, 1.5f, 0.05f, "%.2f"),
        slider("Screen Background", vr_gadget_screen_background, 0.f, 4.f, 0.1f, "%.1f"),
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
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx"),
        slider("Two-Hand Throw Speed", vr_2h_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx"),
        cycle("Throw Gravity", vr_throw_gravity, {{9.81f, "Real"}, {0.f, "Quake"}}),
        slider("Velocity Window", vr_throw_window, 0.04f, 0.3f, 0.01f, "%.2f s")
            .help("Around the release, where the hand's fastest moment sets the throw."),
        slider("Direction Lookback", vr_throw_dir_lookback, 0.f, 0.1f, 0.005f, "%.3f s")
            .help("How far back from that moment the throw's direction is averaged."),
        slider("Lever Arm", vr_throw_lever_arm, 0.f, 0.3f, 0.01f, "%.2f m")
            .help("From the palm to the held object's centre: wrist flicks add speed through it."),
        toggle("Analog Release", vr_throw_release)
            .help("A throw lets go as the grip starts to open, not only once it is released."),
        slider("Max Speed Gain", vr_throw_gain_max, 1.f, 3.f, 0.05f, "%.2fx")
            .help("Extra speed for fast throws, which feel weak at true speed."),
        toggle("Aim Assist", vr_throw_assist)
            .help("Throws close to an enemy's direction bend towards it."),
        slider("Assist Cone", vr_throw_assist_cone, 2.f, 30.f, 1.f, "%.0f deg"),
        slider("Assist Strength", vr_throw_assist_strength, 0.f, 1.f, 0.05f, "%.2f"),
        header("Physics"),
        slider("Bounciness", vr_throw_restitution, 0.f, 0.8f, 0.05f, "%.2f"),
        slider("Friction", vr_throw_friction, 0.f, 1.5f, 0.05f, "%.2f"),
        slider("Max Spin", vr_throw_spin_max, 0.f, 40.f, 1.f, "%.0f rad/s"),
        slider("Spin Drag", vr_throw_spin_drag, 0.f, 2.f, 0.05f, "%.2f"),
        slider("Hitbox", vr_throw_hitbox, 1.f, 12.f, 0.5f, "%.1f").help("Half-size of a thrown weapon's box against monsters."),
        slider("Hit Min Speed", vr_throw_hit_min_speed, 0.f, 600.f, 25.f, "%.0f")
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
        toggle("Drawn In the Hand", vr_carry_local)
            .help("What you carry is drawn in your hand as it is this frame: no lag or lead as you walk or turn. Off: where the server has it."),
        toggle("Fit to the Hand", vr_held_surface_fit)
            .help("A box, backpack or gib you grip sits against your curled fingers, by its drawn shape. Off: it stays where you gripped it."),
        slider("Fit Gap", vr_held_fit_gap, -6.f, 3.f, 0.1f, "%.1f cm")
            .help("Space left between your fingers and what they hold (negative: sunk in). Per model: vr_held_fit_gaps in the console."),
        slider("Push Strength", vr_carry_nudge, 0.f, 2.f, 0.1f, "%.1fx"),
        slider("Box Throw Speed", vr_carry_throw_mult, 0.5f, 3.f, 0.1f, "%.1fx"),
        slider("Box Punch Damage", vr_carry_melee_mult, 1.f, 3.f, 0.1f, "%.1fx").help("Punching with a box in hand."),
        slider("Thrown Box Damage", vr_carry_throw_damage, 0.f, 50.f, 1.f, "%.0f").help("Damage of a box thrown at about 6 m/s; more the faster."),
        header("Armour"),
        cycle("Armour", vr_armor_wear, {{0.f, "Touch takes it"}, {1.f, "Wear by hand"}})
            .help("Wear by hand: grip the armour to carry it and let go of it over your chest to put it on (only if it is better "
                  "than yours). Walking over it no longer takes it. Next map."),
        slider("Armour Size", vr_armor_scale, 0.3f, 1.f, 0.05f, "%.2fx").help("The carried armour's size (1: Quake's, a metre tall). Next map."),
        header("Gibs and Corpses"),
        cycle("Gibs and Heads", vr_grab_gibs, {{0.f, "Left alone"}, {1.f, "Grab by hand"}, {2.f, "Hand and force grab"}})
            .help("Pick up and throw gibs and heads, by reaching for them (or force-grabbing them too)."),
        slider("Thrown Gib Damage", vr_gib_throw_damage, 0.f, 50.f, 1.f, "%.0f").help("Damage of a gib or head thrown at about 6 m/s; more the faster."),
        toggle("Destroy Gibs", vr_gib_destroy)
            .help("Gibs and heads burst in a mist of blood when shot, blown up, struck, or thrown hard at a wall or a monster."),
        slider("Gib Health", vr_gib_health, 1.f, 60.f, 1.f, "%.0f").help("The damage that destroys a gib; a head takes half as much again."),
        slider("Gib Splat Speed", vr_gib_splat_speed, 100.f, 600.f, 25.f, "%.0f")
            .help("Units/s a thrown gib or head must hit a wall or a monster at to burst."),
        toggle("Gib Corpses", vr_corpse_gib)
            .help("Corpses burst into gibs when shot, blown up or struck enough: shotguns, nails, lightning, rockets, fists, melee weapons."),
        slider("Corpse Health", vr_corpse_health, 10.f, 300.f, 10.f, "%.0f")
            .help("The damage that gibs a corpse; a big monster's takes more (an ogre's 1.75 times, a fiend's 2.25, a shambler's 3.5)."),
    };
}

[[nodiscard]] std::vector<Item> pageForceGrab()
{
    return {
        toggle("Force Grab", vr_forcegrab_mode)
            .help("Point an empty hand at an object, pull the trigger, flick the hand: it flies to you. Grip as it arrives "
                  "to catch it."),
        slider("Distance", vr_forcegrab_distance, 100.f, 1500.f, 25.f, "%.0f"),
        slider("Aim Cone", vr_forcegrab_cone, 3.f, 45.f, 1.f, "%.0f deg").help("How far off where the hand points an object may be."),
        slider("Flick Speed", vr_forcegrab_flick_speed, 0.3f, 3.f, 0.1f, "%.1f m/s")
            .help("How fast the hand moves back or up to pull."),
        slider("Flick Turn", vr_forcegrab_flick_turn, 50.f, 800.f, 25.f, "%.0f deg/s")
            .help("Or how fast the fingers swing back or up."),
        slider("Flight Time", vr_forcegrab_time, 0.15f, 1.f, 0.05f, "%.2f s"),
        slider("Flight Speed", vr_forcegrab_speed, 300.f, 3000.f, 100.f, "%.0f")
            .help("Units per extra second of flight: longer pulls fly longer."),
        slider("Arc Height", vr_forcegrab_arc, 0.f, 0.5f, 0.05f, "%.2f"),
        slider("Catch Radius", vr_forcegrab_catch_radius, 4.f, 32.f, 1.f, "%.0f"),
        slider("Catch Early", vr_forcegrab_catch_early, 0.05f, 1.f, 0.05f, "%.2f s")
            .help("How long before it arrives the grip may close to catch it."),
        slider("Catch Late", vr_forcegrab_catch_late, 0.f, 0.5f, 0.05f, "%.2f s"),
        toggle("Pointing Particles", vr_forcegrab_eligible_particles).help("Sparkles on the object an empty hand points at, that it can pull."),
        toggle("Pointing Haptics", vr_forcegrab_eligible_haptics).help("A tick in the hand when it points at a new object it can pull."),
        slider("Outline", "vr_forcegrab_outline", 0.f, 2.f, 0.1f, "%.1f").help("The soft glow round the object a hand points at (0 off)."),
        toggle("Effects", "vr_forcegrab_fx").help("A faint beam to what you point at, a crackling tendril when locked on, a trail behind what flies to you."),
        slider("Ammo/Health Box Size", vr_forcegrabbable_box_scale, 0.1f, 1.f, 0.05f, "%.2f")
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
    const char* label; // its row there (short: right-aligned left of the middle)
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
    {nullptr, "Gore", "Gore", pageGore},
    {nullptr, "Throwing and Physics", "Throwing and Physics", pageThrowing},
    {nullptr, "Carrying and Gibs", "Carrying and Gibs", pageCarrying},
    {nullptr, "Force Grab", "Force Grab", pageForceGrab},

    {"Body and Movement", "Body", "Body", pageBody},
    {nullptr, "Arms and Pauldrons", "Body - Arms and Pauldrons", pageBodyArms},
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
};
constexpr int pageCount = static_cast<int>(sizeof(pages) / sizeof(pages[0]));

std::vector<Item> pageMain()
{
    return {
        header("Comfort"),
        cycle("Turning", vr_snap_turn, {{0.f, "Smooth"}, {30.f, "Snap 30"}, {45.f, "Snap 45"}, {90.f, "Snap 90"}}),
        slider("Turn Speed", vr_turn_speed, 1.f, 8.f, 0.25f, "%.2f"),
        cycle("Move Towards", vr_movement_mode, {{1.f, "Head"}, {0.f, "Off hand"}}),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}}).help("The speed button switches to the other."),
        slider("Stick Deadzone", vr_deadzone, 0.f, 50.f, 5.f, "%.0f%%"),
        toggle("Teleport", vr_teleport_enabled),
        slider("Teleport Range", vr_teleport_range, 100.f, 800.f, 50.f, "%.0f"),
        slider("Room Scale", vr_roomscale_move_mult, 0.5f, 2.f, 0.1f, "%.1fx"),

        header("Body"),
        toggle("Left Handed", vr_lefthanded),
        slider("Height", vr_height_calibration, 1.2f, 2.2f, 0.01f, "%.2f m"),
        action("Set Height Now", calibrateHeight),
        slider("World Scale", vr_world_scale, 0.75f, 1.5f, 0.05f, "%.2f"),
        slider("Floor Offset", vr_floor_offset, -40.f, 10.f, 1.f, "%.0f"),
        toggle("Chest Flashlight", vr_flashlight).help("Trigger with a hand at the torch on your chest switches it; grip takes it."),

        header("Weapons"),
        slider("Gun Angle", vr_gunangle, -30.f, 90.f, 2.5f, "%.1f"),
        slider("Off Hand Angle", vr_offhandpitch, -30.f, 90.f, 2.5f, "%.1f"),
        cycle("Weapon Grip", vr_weapon_grip_mode, {{0.f, "Hold"}, {1.f, "Sticky"}}),
        cycle("Two-Handed", vr_2h_mode, {{0.f, "Off"}, {1.f, "Basic"}, {2.f, "Virtual stock"}}),
        open("Weapon Offsets (Held Weapon)", pageIndex(pageWeaponOffsets)),
        toggle("Two-Handed Hand-Off", vr_2h_handoff).help("Letting go with the hand holding a two-handed weapon leaves it in the other hand: a sword changes hands; a gun hangs from its foregrip until a hand takes its handle."),
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx"),
        cycle("Throw Gravity", vr_throw_gravity, {{9.81f, "Real"}, {0.f, "Quake"}}),
        toggle("Force Grab", vr_forcegrab_mode),
        cycle("Haptics", vr_disablehaptics, {{0.f, "On"}, {1.f, "Off"}}),
        cycle("Crosshair", vr_crosshair, {{0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft laser"}}),
        slider("Crosshair Size", vr_crosshair_size, 0.5f, 8.f, 0.5f, "%.1f"),

        header("Display"),
        cycle("HUD", vr_hud_mode, {{1.f, "Wrist gadget"}, {0.f, "Status bar"}}),
        cycle("Status Bar", vr_sbar_mode, {{1.f, "Off hand"}, {0.f, "Main hand"}}),
        slider("HUD Scale", vr_hud_scale, 0.01f, 0.05f, 0.0025f, "%.4f"),
        slider("Menu Distance", vr_menu_distance, 40.f, 150.f, 5.f, "%.0f"),
        slider("Menu Scale", vr_menu_scale, 0.08f, 0.3f, 0.01f, "%.2f"),
        cycle("Desktop Mirror", vr_mirror, {{0.f, "Off"}, {1.f, "Left eye"}, {2.f, "Both eyes"}}),
        cycle("Body", vr_body_mode, {{0.f, "Off"}, {2.f, "Torso and arms"}, {3.f, "Full body"}}),
        cycle("Build", vr_body_build, {{0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}}),
        slider("Torso Offset", vr_body_torso_back, -0.15f, 0.3f, 0.01f, "%.2f m back"),
        slider("Legs Offset", vr_body_legs_back, -0.15f, 0.3f, 0.01f, "%.2f m back"),
        slider("Shoulders Offset", vr_body_shoulders_back, -0.1f, 0.15f, 0.01f, "%.2f m back"),
        toggle("Holster Models", vr_leg_holster_model_enabled),

        header("Headset"),
        toggle("VR", vr_enabled),
        action("Restart VR", restartVr),
        cycle("OpenXR Runtime", vr_xr_runtime, {{0.f, "System default"}, {1.f, "Virtual Desktop (VDXR)"}, {2.f, "SteamVR"}})
            .help("Which OpenXR runtime runs the headset; VR restarts. VDXR skips SteamVR (keep Virtual Desktop's 'Emulate Index controllers' off)."),
        slider("Render Scale", vr_render_scale, 0.5f, 1.5f, 0.05f, "%.2f")
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
        list.push_back(open(pages[p].label, p));
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

void weaponOffsetsReset()
{
    weapons::resetSlotToDefaults(weaponOffsetsSlot);
    weaponOffsetsStale = true;
}

void weaponOffsetsPrint()
{
    weapons::printSlot(weaponOffsetsSlot);
}

// The hotspot being edited (vr_weapon_hotspot, 1..4) and its type, as the page was built: a change rebuilds it.
int weaponOffsetsHotspot = -1;
int weaponOffsetsHotspotType = -1;

[[nodiscard]] int editedHotspot()
{
    return CLAMP(1, static_cast<int>(vr_weapon_hotspot.value), weapons::maxHotspots) - 1;
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
    if(!view::hotspotAt(weaponOffsetsHand, hands::current().pos[1 - weaponOffsetsHand], p))
    {
        Con_Printf("Hold the weapon in the %s hand to place its hotspot with the other hand.\n",
            weaponOffsetsHand == 1 ? "main" : "off");
        return;
    }
    h.type = weapons::HotspotType::Grip;
    h.pos = p;
    weapons::setHotspot(slot, editedHotspot(), h);
    weaponOffsetsStale = true;
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
    const auto s = [&](const char* label, Key key, float min, float max, float step, const char* format) {
        return slider(label, weapons::cvar(slot, key), min, max, step, format);
    };
    const bool fist = slot == weapons::fistSlot(); // the empty hand's "weapon" is the hand model
    list = {
        header(title.c_str()),
        action("Edit the Other Hand's Weapon", weaponOffsetsOtherHand)
            .help("The page shows the weapon the hand held when it was opened: reopen it after changing weapons."),
        header(fist ? "The Hand" : "Weapon in the Hand"),
        s("Offset X (forward)", Key::OffsetX, -30.f, 30.f, 0.1f, "%.2f")
            .help(fist ? "Moves the drawn hand." :
                         "Where the weapon sits in the hand (the hand is where the controller is, and its fingers wrap "
                         "the weapon's grip). Doesn't change where it aims."),
        s("Offset Y (left)", Key::OffsetY, -30.f, 30.f, 0.1f, "%.2f"),
        s("Offset Z (up)", Key::OffsetZ, -30.f, 30.f, 0.1f, "%.2f"),
        s("Pitch", Key::Pitch, -180.f, 180.f, 0.5f, "%.1f").help("How the weapon is turned in the hand."),
        s("Yaw", Key::Yaw, -180.f, 180.f, 0.5f, "%.1f"),
        s("Roll", Key::Roll, -180.f, 180.f, 0.5f, "%.1f"),
        s("Scale", Key::Scale, 0.1f, 3.f, 0.01f, "%.2f"),
        cycle("Hide Hand", weapons::cvar(slot, Key::HideHand), {{0.f, "No"}, {1.f, "Yes"}}),
    };
    if(!fist)
    {
        const char* fingerHelp = "Closes (+) or opens (-) this finger on top of how it wraps the weapon on its own "
                                 "(a share of a full curl).";
        list.insert(list.end(), {
            header("Fingers on the Weapon"),
            s("Thumb", Key::FingerThumbBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Index Finger", Key::FingerIndexBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Middle Finger", Key::FingerMiddleBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Ring Finger", Key::FingerRingBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Little Finger", Key::FingerPinkyBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Thumb X (forward)", Key::FingerThumbX, -4.f, 4.f, 0.05f, "%+.2f").help("Moves the thumb on the hand."),
            s("Thumb Y (palm)", Key::FingerThumbY, -4.f, 4.f, 0.05f, "%+.2f"),
            s("Thumb Z (up)", Key::FingerThumbZ, -4.f, 4.f, 0.05f, "%+.2f"),
            header("Muzzle"),
            s("Muzzle X", Key::MuzzleOffsetX, -30.f, 30.f, 0.1f, "%.2f").help("Where shots and the muzzle flash start, from the muzzle vertex."),
            s("Muzzle Y", Key::MuzzleOffsetY, -30.f, 30.f, 0.1f, "%.2f"),
            s("Muzzle Z", Key::MuzzleOffsetZ, -30.f, 30.f, 0.1f, "%.2f"),
        });

        // The two-handed grips: the hotspot being edited.
        const int index = editedHotspot();
        const weapons::Hotspot h = weapons::hotspot(slot, index);
        weaponOffsetsHotspot = index;
        weaponOffsetsHotspotType = static_cast<int>(h.type);
        const auto hk = [&](int field) { return weapons::cvar(slot, weapons::hotspotKey(index, field)); };
        list.insert(list.end(), {
            header("Other Hand's Grips (Hotspots)"),
            cycle("Hotspot", vr_weapon_hotspot, {{1.f, "1"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}})
                .help("Where the other hand may hold the weapon: it takes the one nearest it, less its bias. Pick one to edit."),
            cycle("Type", hk(0), {{0.f, "None"}, {1.f, "Grip"}, {2.f, "Blade"}})
                .help("Grip: a point (a foregrip, a pump, a magazine) the hand is drawn on. Blade: the half-sword grip along "
                      "the blade."),
        });
        if(h.type == weapons::HotspotType::Blade)
        {
            list.push_back(slider("Along the Blade", hk(1), 0.f, 1.f, 0.01f, "%.2f")
                               .help("Where on the blade the grip is centred: a share of the way from the hand to the tip."));
        }
        else
        {
            list.insert(list.end(), {
                slider("Hotspot X", hk(1), -40.f, 40.f, 0.1f, "%.2f").help("The grip's point, in the weapon's model units."),
                slider("Hotspot Y", hk(2), -40.f, 40.f, 0.1f, "%.2f"),
                slider("Hotspot Z", hk(3), -40.f, 40.f, 0.1f, "%.2f"),
                action("Put It Where the Other Hand Is", weaponOffsetsHotspotAtHand)
                    .help("Makes this hotspot a grip at the other hand, as it is now."),
            });
        }
        list.insert(list.end(), {
            slider("Bias", hk(4), 0.f, 10.f, 0.1f, "%.1f").help("Units taken off its distance: larger, easier to take than the others."),
            action("Remove This Hotspot", weaponOffsetsHotspotRemove),
            toggle("Show Hotspots", vr_show_weapon_hotspots).help("Marks the held weapons' hotspots (the edited one white)."),
            header("Two-Handed Aim"),
            s("Aim Offset X", Key::TwoHOffsetX, -30.f, 30.f, 0.1f, "%.2f")
                .help("Moves the point the aim is taken from, in the holding hand's frame (nothing drawn moves)."),
            s("Aim Offset Y", Key::TwoHOffsetY, -30.f, 30.f, 0.1f, "%.2f"),
            s("Aim Offset Z", Key::TwoHOffsetZ, -30.f, 30.f, 0.1f, "%.2f"),
            s("Aim Pitch", Key::TwoHPitch, -180.f, 180.f, 0.5f, "%.1f")
                .help("Turns the two-handed aim (a sword: its blade's direction in the model)."),
            s("Aim Yaw", Key::TwoHYaw, -180.f, 180.f, 0.5f, "%.1f"),
            s("Aim Roll", Key::TwoHRoll, -180.f, 180.f, 0.5f, "%.1f"),
            header("Ammo Screen"),
            s("Screen X", Key::WpnTextX, -20.f, 20.f, 0.05f, "%.2f"),
            s("Screen Y", Key::WpnTextY, -20.f, 20.f, 0.05f, "%.2f"),
            s("Screen Z", Key::WpnTextZ, -20.f, 20.f, 0.05f, "%.2f"),
            s("Screen Pitch", Key::WpnTextPitch, -180.f, 180.f, 0.5f, "%.1f"),
            s("Screen Yaw", Key::WpnTextYaw, -180.f, 180.f, 0.5f, "%.1f"),
            s("Screen Roll", Key::WpnTextRoll, -180.f, 180.f, 0.5f, "%.1f"),
            s("Screen Scale", Key::WpnTextScale, 0.05f, 3.f, 0.05f, "%.2f"),
        });
    }
    list.insert(list.end(), {
        header("This Weapon"),
        s("Weight", Key::Weight, 0.f, 1.f, 0.05f, "%.2f").help("How much the weapon lags the hand."),
        action("Print Changes to Console", weaponOffsetsPrint)
            .help("Prints this weapon's settings that differ from the defaults, ready to be made the shipped defaults."),
        action("Reset This Weapon", weaponOffsetsReset).help("Every setting of this weapon back to its default."),
    });
    return list;
}

// Built on first use (cvars looked up by name exist by then); items without their cvar dropped.
[[nodiscard]] const std::vector<Item>& items(int page)
{
    static std::vector<Item> built[pageCount];
    static bool done[pageCount]{};
    if(pages[page].build == pageWeaponOffsets && weaponOffsetsSlot >= 0 &&
        (editedHotspot() != weaponOffsetsHotspot ||
            static_cast<int>(weapons::hotspot(weaponOffsetsSlot, editedHotspot()).type) != weaponOffsetsHotspotType))
    {
        weaponOffsetsStale = true; // another hotspot picked, or its type changed
    }
    if(pages[page].build == pageWeaponOffsets && weaponOffsetsStale)
    {
        weaponOffsetsStale = false;
        done[page] = false;
        built[page].clear();
    }
    if(!done[page])
    {
        done[page] = true;
        for(Item& item : pages[page].build())
        {
            if(item.kind == Item::Header || item.kind == Item::Action || item.cvar)
            {
                built[page].push_back(std::move(item));
            }
        }
    }
    return built[page];
}

int page = PageMain;
int parentPage[pageCount]{};
int cursors[pageCount]{};
int scrolls[pageCount]{};
bool sliderGrab = false; // a slider follows the mouse while its button is held
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
        if(item.helpText)
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
        if(list[i].kind != Item::Header)
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
    } while(list[i].kind == Item::Header && i != cursor);
    cursor = i;
}

// Shows `target`, with its cursor on a setting.
void showPage(int target)
{
    page = target;
    if(pages[page].build == pageWeaponOffsets)
    {
        weaponOffsetsStale = true; // the weapon in hand now
        cursors[page] = 0;
        scrolls[page] = 0;
    }
    const auto& list = items(page);
    if(list[cursors[page]].kind == Item::Header)
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

void change(const Item& item, int dir)
{
    switch(item.kind)
    {
        case Item::Slider:
        {
            float v = item.cvar->value + dir * item.step;
            v = std::round(v / item.step) * item.step;
            v = CLAMP(item.min, v, item.max);
            Cvar_SetValueQuick(item.cvar, item.negativeLabel && v < 0.f ? -1.f : v);
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
                if(item.page >= 0)
                {
                    openPage(item.page);
                    return;
                }
                item.action();
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
        while(list[cursor].kind == Item::Header && cursor + dir >= scroll && cursor + dir < scroll + rows)
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
            const float range = (item.cvar->value - item.min) / (item.max - item.min);
            M_DrawSlider(midPos, y, CLAMP(0.f, range, 1.f), buf);
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

// menu_vr [page]: the VR Settings, or one of its pages (1: Advanced VR Options); menu_vr list:
// the pages' numbers.
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
        }
    }
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

    if(cursor < n && list[cursor].cvar == &vr_render_scale)
    {
        drawHelp(renderScaleHelp());
    }
    else if(cursor < n && list[cursor].helpText)
    {
        drawHelp(list[cursor].helpText);
    }
}

extern "C" void VR_Menu_Key(int key)
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

        case K_LEFTARROW: change(list[cursor], -1); break;
        case K_RIGHTARROW: change(list[cursor], 1); break;

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
                    sliderGrab = true;
                    setSliderAt(list[cursor], m_mousex);
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
        else
        {
            setSliderAt(list[cursor], cx);
        }
        return;
    }

    const int i = rowAt(cy);
    if(i < 0 || list[i].kind == Item::Header || i == cursor)
    {
        return;
    }
    cursor = i;
    if(ui_mouse_sound.value)
    {
        S_LocalSound("misc/menu1.wav");
    }
}
