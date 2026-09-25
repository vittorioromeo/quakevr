// vr_menu.cpp -- the "VR Settings" pages (Options > VR Settings), drawn like Ironwail's options
// pages: scrolling lists of labelled settings, changed with left/right (the sticks in VR), with
// actions on enter (A). "Advanced VR Options" at the bottom opens a list of further pages: the old
// Quake VR settings pages (vr_menu_pages.inc) and the new body, throwing and force grab tweaks.
// Escape (B) goes back a page. With the mouse (and the VR laser pointer, vr_menuui.cpp): the row under
// it is selected, a click picks it, and a slider is set where it is clicked and dragged.

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"

#include <cmath>
#include <cstring>
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
        header("Parry and Bash"),
        toggle("Unarmed Parry", vr_parry_unarmed).help("Cross your arms in an X in front of you to block a blow with your forearms."),
        slider("Unarmed Parry Reduction", vr_parry_unarmed_reduction, 0.f, 1.f, 0.05f, "%.2f"),
        toggle("Bash", vr_bash).help("Hold a guard (a weapon level across in front, or both hands together), then push it forward: knocks monsters back and staggers them. One open hand, palm ahead, shoves half as hard."),
        slider("Bash Speed", vr_bash_speed, 0.8f, 3.f, 0.1f, "%.1f m/s").help("How fast the guard must be pushed forward (less than a blow needs)."),
        slider("Bash Damage", vr_bash_damage, 0.f, 40.f, 1.f, "%.0f"),
        slider("Bash Push", vr_bash_push, 0.f, 3.f, 0.05f, "%.2fx").help("How far a bash or shove throws what it hits (times Knockback)."),
        header("Playtesting"),
        toggle("Voice Notes", vr_notes).help("Raise your off hand to your mouth and hold Y to record a note, with a screenshot and where you are; they go to quakevr/notes."),
        header("Feel"),
        toggle("Bat Back Projectiles", vr_deflect).help("Swing a weapon (or a fist) through a monster's spike, laser, spit or grenade to send it back where your hand points."),
        toggle("Explosion Rumble", vr_explosion_rumble).help("Explosions near you rumble in your hands."),
        toggle("Low Health Heartbeat", vr_heartbeat).help("A heartbeat in your hands when your health is low."),
        header("Headbutt"),
        toggle("Headbutt", vr_headbutt).help("Lunge your head at something to headbutt it."),
        slider("Headbutt Speed", vr_headbutt_speed, 0.4f, 3.f, 0.05f, "%.2f m/s").help("How fast the head must lunge (towards where you look)."),
        slider("Headbutt Damage", vr_headbutt_damage, 5.f, 100.f, 1.f, "%.0f"),
        header("Knights' Swords"),
        slider("Knights Drop Swords", vr_sword_drop, 0.f, 1.f, 0.05f, "%.2f").help("Chance a dying knight or hell knight drops its sword, a melee weapon you can pick up."),
        slider("Sword Damage", vr_sword_damage_mult, 0.5f, 3.f, 0.05f, "%.2fx").help("A sword swing's damage over the axe's (the hell knight's sword: 25% more)."),
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
        toggle("Show Armour and Wounds", vr_body_state)
            .help("The armour you wear plates your torso; your arms and hands get bloodier as you are hurt."),
        toggle("Wounds Drip Blood", vr_body_blood)
            .help("Blood drips from your wounded arms and hands, faster when badly hurt or just hit, and splashes on the floor."),
        toggle("Show Powerups", vr_body_powerups)
            .help("Quad damage sparks around your hands, the pentagram makes you glow, the ring fades you."),
        toggle("Pauldrons", vr_body_pauldrons).help("Leather pads over the shoulders and the tops of the arms, as the Quake ranger wears."),
        cycle("Pauldron Style", vr_body_pauldron_style, {{0.f, "Ranger leather"}, {1.f, "Armour colour"}, {2.f, "Steel"}})
            .help("Armour colour: green, yellow or red as the armour you wear (leather without)."),
        slider("Pauldron Size", vr_body_pauldron_size, 0.5f, 1.5f, 0.05f, "%.2fx"),
        slider("Pauldron Follows Arm", vr_body_pauldron_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the shoulder cap turns with the upper arm (the lower plates follow the arm fully)."),
        slider("Pauldron Forward", vr_body_pauldron_forward, -0.05f, 0.05f, 0.005f, "%.3f m"),
        slider("Pauldron Up", vr_body_pauldron_up, -0.05f, 0.05f, 0.005f, "%.3f m"),
        slider("Pauldron Out", vr_body_pauldron_out, -0.05f, 0.05f, 0.005f, "%.3f m"),
        toggle("Anchors Follow Body", vr_body_anchors)
            .help("Holsters, the virtual stock and hand collisions follow the body's lean and crouch."),
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
        header("Flashlight"),
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
        header("Colours"),
        slider("Screen Hue", vr_gadget_screen_hue, 0.f, 355.f, 5.f, "%.0f")
            .help("The screen's colour: 128 green, 40 amber, 200 blue, 0 red."),
        slider("Screen Brightness", vr_gadget_screen_brightness, 0.3f, 1.5f, 0.05f, "%.2f"),
        slider("Screen Background", vr_gadget_screen_background, 0.f, 4.f, 0.1f, "%.1f"),
        slider("Casing Tint", vr_gadget_tint, 0.f, 1.f, 0.05f, "%.2f").help("0 keeps the casing's own olive drab."),
        slider("Casing Tint Hue", vr_gadget_tint_hue, 0.f, 355.f, 5.f, "%.0f"),
        header("Screen"),
        toggle("Level and Stats", vr_gadget_show_level),
        slider("Screen Light", vr_gadget_light, 0.f, 3.f, 0.1f, "%.1fx")
            .help("The screen casts a light in its colour the way it faces, and a faint one on your hand (0 off)."),
        slider("CRT Look", vr_gadget_crt, 0.f, 2.f, 0.1f, "%.1fx")
            .help("Scanlines, a slight flicker, faint static and now and then a glitch (0 off)."),
        slider("Screen Glow", vr_screen_glow, 0.f, 3.f, 0.1f, "%.1fx")
            .help("The gadget's and your weapons' screens glow softly round their edges (0 off)."),
        slider("Text Glow", vr_screen_text_glow, 0.f, 3.f, 0.1f, "%.1fx")
            .help("The text, numbers and icons on those screens glow: bright whitish cores, a soft halo (0 off)."),
        cycle("Messages", vr_notify_wrist, {{1.f, "Over the gadget"}, {2.f, "Both"}, {0.f, "In view"}})
            .help("The console's messages float in a small log over the gadget, or at the top of the view."),
        slider("Message Time", vr_notify_wrist_time, 2.f, 30.f, 1.f, "%.0f s")
            .help("How long a message stays in the gadget's log."),
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
        header("Carrying Boxes"),
        toggle("Carry Ammo and Health", vr_carry)
            .help("Grip a box or a backpack to carry it, push it with a hand or gun. Off: touching takes it."),
        cycle("Gibs and Heads", vr_grab_gibs, {{0.f, "Left alone"}, {1.f, "Grab by hand"}, {2.f, "Hand and force grab"}})
            .help("Pick up and throw gibs and heads, by reaching for them (or force-grabbing them too)."),
        cycle("Take a Box", vr_carry_take, {{0.f, "At a holster"}, {1.f, "Trigger"}, {2.f, "Either"}})
            .help("At a holster: let go of it at a hip or shoulder holster to put it in your pack."),
        toggle("Drawn In the Hand", vr_carry_local)
            .help("What you carry is drawn in your hand as it is this frame: no lag or lead as you walk or turn. Off: where the server has it."),
        slider("Push Strength", vr_carry_nudge, 0.f, 2.f, 0.1f, "%.1fx"),
        slider("Box Throw Speed", vr_carry_throw_mult, 0.5f, 3.f, 0.1f, "%.1fx"),
        slider("Box Punch Damage", vr_carry_melee_mult, 1.f, 3.f, 0.1f, "%.1fx").help("Punching with a box in hand."),
        slider("Thrown Box Damage", vr_carry_throw_damage, 0.f, 50.f, 1.f, "%.0f").help("Damage of a box thrown at about 6 m/s; more the faster."),
        slider("Thrown Gib Damage", vr_gib_throw_damage, 0.f, 50.f, 1.f, "%.0f").help("Damage of a gib or head thrown at about 6 m/s; more the faster."),
        toggle("Destroy Gibs", vr_gib_destroy)
            .help("Gibs and heads burst in a mist of blood when shot, blown up, struck, or thrown hard at a wall or a monster."),
        slider("Gib Health", vr_gib_health, 1.f, 60.f, 1.f, "%.0f").help("The damage that destroys a gib; a head takes half as much again."),
        slider("Gib Splat Speed", vr_gib_splat_speed, 100.f, 600.f, 25.f, "%.0f")
            .help("Units/s a thrown gib or head must hit a wall or a monster at to burst."),
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
        toggle("Pointing Particles", vr_forcegrab_eligible_particles),
        toggle("Pointing Haptics", vr_forcegrab_eligible_haptics),
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
    const char* title;
    std::vector<Item> (*build)();
};

[[nodiscard]] std::vector<Item> pageMain();
[[nodiscard]] std::vector<Item> pageAdvanced();

const Page pages[] = {
    {"VR Settings", pageMain},
    {"Advanced VR Options", pageAdvanced},
    {"Play", pagePlay},
    {"Gameplay", pageGameplay},
    {"Body", pageBody},
    {"Wrist Gadget", pageGadget},
    {"Throwing and Physics", pageThrowing},
    {"Force Grab", pageForceGrab},
    {"Swimming", pageSwimSettings},
    {"Menu", pageMenuSettings},
    {"Crosshair", pageCrosshairSettings},
    {"Particles", pageParticleSettings},
    {"Locomotion", pageLocomotionSettings},
    {"Hand/Gun Calibration", pageHandGunCalibration},
    {"Player Calibration", pagePlayerCalibration},
    {"Melee", pageMeleeSettings},
    {"Aiming", pageAimingSettings},
    {"Immersion", pageImmersionSettings},
    {"Graphics", pageGraphicalSettings},
    {"Status Bar", pageHudConfiguration},
    {"Hotspots", pageHotspotSettings},
    {"Transparency", pageTransparencyOptions},
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
        toggle("Hide Lens Corners", vr_visibility_mask)
            .help("Skip the pixels the lenses never show (if the headset gives them): faster, looks the same. Black corners in the desktop mirror."),

        header("More"),
        open("Advanced VR Options", PageAdvanced),
    };
}

std::vector<Item> pageAdvanced()
{
    std::vector<Item> list{header("The port's own")};
    for(int p = PageFirstAdvanced; p < pageCount; p++)
    {
        if(pages[p].build == pageMenuSettings)
        {
            list.push_back(header("Quake VR's"));
        }
        list.push_back(open(pages[p].title, p));
    }
    return list;
}

// Built on first use (cvars looked up by name exist by then); items without their cvar dropped.
[[nodiscard]] const std::vector<Item>& items(int page)
{
    static std::vector<Item> built[pageCount];
    static bool done[pageCount]{};
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

constexpr int listTop = 36;
constexpr int helpTop = 164; // four lines of help under the list, on pages with any
constexpr int midPos = 204;  // as Ironwail's OPTIONS_MIDPOS

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
    return ((hasHelp(list) ? helpTop - 4 : 200 - 8) - listTop) / 8;
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
            Cvar_SetValueQuick(item.cvar, CLAMP(item.min, v, item.max));
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
    const int row = static_cast<int>(std::floor((cy - listTop) / 8.f));
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
    const float yrel = cy - listTop - height * 4.f;
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
            q_snprintf(buf, sizeof(buf), item.format, item.cvar->value);
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
        M_PrintWhite((320 - 8 * n) / 2, helpTop + line * 8, buf);
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

// menu_vr [page]: the VR Settings, or one of its pages (1: Advanced VR Options).
void qvr::menu::command_f()
{
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

    M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
    qpic_t* title = Draw_CachePic("gfx/p_option.lmp");
    M_DrawPic((320 - title->width) / 2, 4, title);
    const char* name = pages[page].title;
    M_PrintWhite((320 - 8 * static_cast<int>(strlen(name))) / 2, 28, name);

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
        drawItem(list[i], listTop + (i - scroll) * 8, i == cursor);
    }

    if(int y, height; scrollbar(n, rows, y, height))
    {
        scrollbarX = q_min(midPos + 188, static_cast<int>(glcanvas.right) - 16);
        M_DrawTextBox(scrollbarX - 4, listTop + y - 4, 0, height - 1);
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
