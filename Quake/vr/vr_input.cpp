// vr_input.cpp -- controller input: keys, locomotion, turning, haptics.
//
// Controller buttons are Quake keys, so everything they do comes from bindings (defaults in
// quakevr/default.cfg) and can be rebound -- with aliases -- from the console or the bindings
// menu. They reuse Ironwail's gamepad keys: the main hand (always the right controller)
// is the "right" half of a gamepad (RT, RB, A, B, RS), the off hand the "left" half (LT, LB, X,
// Y, LS). Menus understand these keys already.
//
// The left stick moves (analog, see VR_AdjustMove; vr_stick_swap 1: the right one,
// hands::moveHand); the other stick turns, and pushed up or down it is DPAD UP/DOWN. In
// menus the moving stick is the DPAD; the turning one
// only scrolls a page with a scrollbar or is DPAD UP/DOWN (never left/right: it doesn't change
// settings); the menu button closes the menu from any page.

#include "vr_testkit.hpp"
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_chainsaw.hpp"
#include "vr_magholster.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_input.hpp"
#include "vr_doom.hpp"
#include "vr_main.hpp"
#include "vr_menuui.hpp"
#include "vr_motion.hpp"
#include "vr_holstersetup.hpp"
#include "vr_bladeedit.hpp"
#include "vr_poselive.hpp"
#include "vr_posestudio.hpp"
#include "vr_posing.hpp"
#include "vr_progs.hpp"
#include "vr_voicenotes.hpp"
#include "vr_seated.hpp"
#include "vr_timescale.hpp"
#include "vr_flashlight.hpp"
#include "vr_holsterzones.hpp"
#include "vr_toolgun.hpp"
#include "vr_arenacards.hpp"

#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Copysign.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"


extern "C" qboolean keydown[MAX_KEYS]; // keys.c
extern "C" cvar_t sv_maxspeed;          // sv_user.c: the stick's top speed fits it (moveTopSpeed)
extern "C" cvar_t sv_accelerate, sv_stopspeed, sv_friction; // sv_user.c, sv_phys.c: the stick's least speed (moveMinSpeed)

using namespace qvr;

namespace
{

// A take playing back (vr_motion_play): the controllers' key commands go ahead of whatever waits in the
// command buffer (a script's remaining commands and waits), as they run at once in the headset; else a grip
// pressed in a replay would take its weapon only after the script. In game only (menus as usual).
za::String playbackCommands;

void keyEvent(int key, bool down)
{
    if(!motion::playing() || key_dest != key_game)
    {
        Key_Event(key, down);
        return;
    }
    const bool was = keydown[key] != 0;
    keydown[key] = down;
    const char* kb = keybindings[key];
    if(!kb || down == was)
    {
        return;
    }
    if(down)
    {
        playbackCommands += kb[0] == '+' ? za::String{va("%s %i", kb, key)} : za::String{kb};
        playbackCommands += '\n';
    }
    else if(kb[0] == '+')
    {
        playbackCommands += va("-%s %i", kb + 1, key);
        playbackCommands += '\n';
    }
}

struct ButtonKeys
{
    bool HandInput::*button;
    int key[HAND_COUNT]; // off hand, main hand
};

constexpr ButtonKeys buttonKeys[] = {
    {&HandInput::trigger, {K_LTRIGGER, K_RTRIGGER}},
    {&HandInput::grip, {K_LSHOULDER, K_RSHOULDER}},
    {&HandInput::primary, {K_XBUTTON, K_ABUTTON}},
    {&HandInput::secondary, {K_YBUTTON, K_BBUTTON}},
    {&HandInput::stickClick, {K_LTHUMB, K_RTHUMB}},
};

// vr_debug_buttons: a controller button pressed or let go (the hand, the button, its key and what the key is bound to),
// and what took it before the game got its key (`takenBy`; nullptr: the game got it). The face buttons also drive the
// grappling hook while the game has the keys (secondaryHeld, primaryHeld: its reel and unreel).
void logButton(int h, const ButtonKeys& b, bool now, int key, const char* takenBy)
{
    if(!vr_debug_buttons.value)
    {
        return;
    }
    const char* name = b.button == &HandInput::trigger    ? "trigger"
                       : b.button == &HandInput::grip      ? "grip"
                       : b.button == &HandInput::primary   ? (h == HAND_MAIN ? "lower face button (A)" : "lower face button (X)")
                       : b.button == &HandInput::secondary ? (h == HAND_MAIN ? "upper face button (B)" : "upper face button (Y)")
                                                           : "stick click";
    const char* kb = keybindings[key];
    Con_Printf("VR buttons: %s hand's %s %s: key %s (bound to \"%s\")%s%s%s\n", h == HAND_MAIN ? "main" : "off", name,
        now ? "pressed" : "let go", Key_KeynumToString(key), kb ? kb : "", takenBy ? ", taken by " : "", takenBy ? takenBy : "",
        !takenBy && key_dest != key_game ? " (not in the game: no reel or unreel)" : "");
}

// A stick direction acting as a key: pressed past 0.7, released below 0.5, auto-repeating in
// menus.
struct StickKey
{
    int key;
    bool down{false};
    double nextRepeat{0.0};
};

StickKey stickKeys[] = {{K_DPAD_UP}, {K_DPAD_DOWN}, {K_DPAD_LEFT}, {K_DPAD_RIGHT}};

[[nodiscard]] posing::Button posingButton(bool HandInput::*button)
{
    return button == &HandInput::trigger     ? posing::Button::Trigger
           : button == &HandInput::grip      ? posing::Button::Grip
           : button == &HandInput::primary   ? posing::Button::Primary
           : button == &HandInput::secondary ? posing::Button::Secondary
                                             : posing::Button::StickClick;
}

// The menu button is Escape: it opens and closes the menu and can never be unbound. In a menu it goes back to the game
// at once, from any page (menuui::backToGame: the menu opens again on that page); B and Y go back a page (NOTES.md
// vrfiringrange_2026-09-30_00-21-35). Waiting for a key to bind, it is Escape (the binding cancelled).
void escape()
{
    Key_Event(K_ESCAPE, true);
    Key_Event(K_ESCAPE, false);
}

InputState previous; // last frame's input

void menuButton(int hand, bool now, bool before, const char* source)
{
    if(!now || before)
    {
        return;
    }
    const bool closes = key_dest == key_menu && m_state != m_none && !M_WaitingForKeyBinding();
    if(vr_debug_buttons.value)
    {
        Con_Printf("VR buttons: %s hand's %s: %s\n", hand == HAND_MAIN ? "main" : "off", source,
            closes ? "closes the menu" : key_dest == key_game ? "opens the menu" : "Escape");
    }
    if(closes)
    {
        menuui::backToGame(hand);
    }
    else
    {
        escape();
    }
}

// vr_menu_button: the menu without the controller's menu button, which some controllers and runtimes never pass on (a
// player's Pico Neo 3 under SteamVR). Each way is a press of the menu button (menuButton, and the posing mode's): it
// opens the menu, or closes it from any page.
// - 1 and up: both stick clicks together, the second within chordWindow of the first (or in the same frame). That second
//   click (press and release) is the menu's: the game never gets it. The first went to the game as ever (+speed or
//   +reloadmain, or bullet time's stick).
// - 2: or Y (the off hand's upper button) held holdSeconds in the game; 3: B (the main hand's) so. The press reached the
//   game first (a weapon switch), as a tap must not wait.
// - 4: or Y pressed: Y is taken whole (press and release), before the posing mode, the toolgun or a voice note.
// Never while a take plays back (vr_motion_play: its sticks are the take's).
constexpr double chordWindow = 0.3;
constexpr double holdSeconds = 0.8;

struct MenuGestures
{
    double stickDown[HAND_COUNT]{};  // when each stick click went down (realtime)
    bool stickTaken[HAND_COUNT]{};   // that stick click is the chord's (its release goes nowhere either)
    bool yTaken{false};              // vr_menu_button 4: Y held as the menu
    double holdSince{-1.0};          // vr_menu_button 2, 3: the held button's press in the game (-1: none)
    bool holdDone{false};            // this hold has opened the menu, or began outside the game
    bool press[HAND_COUNT]{};        // this frame: a gesture pressed the menu with this hand
    const char* source[HAND_COUNT]{}; // ... which (vr_debug_buttons)
    bool menuWas[HAND_COUNT]{};      // last frame's menu press, the button's or a gesture's
};
MenuGestures menuGestures;

[[nodiscard]] int menuButtonMode()
{
    return motion::playing() ? 0 : CLAMP(0, static_cast<int>(vr_menu_button.value), 4);
}

// A button of hand `h` changed (`now`): true when a menu gesture takes it (the game never gets it).
[[nodiscard]] bool menuGestureButton(const InputState& in, int h, bool HandInput::*button, bool now)
{
    MenuGestures& g = menuGestures;
    const int mode = menuButtonMode();
    if(button == &HandInput::stickClick)
    {
        const int o = 1 - h;
        if(!now)
        {
            const bool taken = g.stickTaken[h];
            g.stickTaken[h] = false;
            return taken;
        }
        g.stickDown[h] = realtime;
        if(g.stickTaken[h])
        {
            return true; // (the chord's other click, in the same frame)
        }
        const bool otherHeld = in.hands[o].stickClick && !g.stickTaken[o];
        const bool otherJust = otherHeld && (!previous.hands[o].stickClick || realtime - g.stickDown[o] <= chordWindow);
        if(mode < 1 || !otherJust)
        {
            return false;
        }
        g.stickTaken[h] = true;
        if(!previous.hands[o].stickClick)
        {
            g.stickTaken[o] = true; // (both in this frame: the other's press, still to come, is the chord's too)
        }
        g.press[h] = true;
        g.source[h] = "both sticks clicked";
        return true;
    }
    if(button == &HandInput::secondary && h == HAND_OFF)
    {
        if(now && mode == 4)
        {
            g.yTaken = true;
            g.press[h] = true;
            g.source[h] = "Y (vr_menu_button 4)";
            return true;
        }
        if(!now && g.yTaken)
        {
            g.yTaken = false;
            return true;
        }
    }
    return false;
}

// Once a frame, before the buttons: vr_menu_button 2 and 3, Y or B held in the game (as the game got it: not a voice
// note's, the flashlight's, the posing mode's...).
void menuHold(const bool secondaryDown[HAND_COUNT])
{
    MenuGestures& g = menuGestures;
    const int mode = menuButtonMode();
    const int h = mode == 2 ? HAND_OFF : HAND_MAIN;
    if((mode != 2 && mode != 3) || !secondaryDown[h])
    {
        g.holdSince = -1.0;
        g.holdDone = false;
        return;
    }
    if(g.holdSince < 0.0)
    {
        g.holdSince = realtime;
        g.holdDone = key_dest != key_game; // (a hold that began in a menu: B and Y go back a page there)
    }
    if(!g.holdDone && key_dest == key_game && realtime - g.holdSince >= holdSeconds)
    {
        g.holdDone = true;
        g.press[h] = true;
        g.source[h] = h == HAND_OFF ? "Y held" : "B held";
    }
}

// The main hand's stick scrolls the menu page (menuui::scrollStick) from when it is pushed on a
// page that scrolls until it is let go back to the middle.
bool mainStickScrolls = false;

// Each hand's upper face button (B/Y) held, as a key the game got (not taken by the posing mode, a voice note or the
// flashlight): the grappling hook reels while it is (secondaryHeld, sent to the QC with the move).
bool secondaryDown[HAND_COUNT] = {};
// And the lower one (A/X): the hook unreels while it is (primaryHeld).
bool primaryDown[HAND_COUNT] = {};

glm::vec2 moveAxes{0.f};
bool snapTurnArmed = true;

// The comfort vignette (vr_comfort_vignette): how much the sticks move and turn you now (0 .. 1, eased), this frame's
// smooth turn (0 .. 1), and the seconds left of a snap turn's pulse.
struct ComfortMotion
{
    float amount{0.f};
    float turn{0.f};
    float snapPulse{0.f};
};
ComfortMotion comfortMotion;

struct PendingHaptic
{
    double time;
    int hand;
    float seconds;
    float frequency;
    float amplitude;
};

za::Vector<PendingHaptic> pendingHaptics;

[[nodiscard]] float deadzone(float v)
{
    const float dz = CLAMP(0.f, vr_deadzone.value / 100.f, 0.9f);
    if(za::fabs(v) < dz)
    {
        return 0.f;
    }
    return (v - za::copysign(dz, v)) / (1.f - dz);
}

// Thumbstick locomotion (ROUND22.md "Thumbstick locomotion response"). The moving stick's radial deadzone: the push's
// length past vr_deadzone (inner, %) up to 1 - vr_deadzone_outer (%) rescaled onto 0..1 (just past the inner one
// starts from 0; the last outer % is full), its direction kept: a diagonal as a straight push, and a push longer than 1
// (some sticks' corners) as 1. The menus' and turning's per-axis deadzone() stays.
[[nodiscard]] glm::vec2 moveStick(glm::vec2 stick)
{
    const float inner = CLAMP(0.f, vr_deadzone.value / 100.f, 0.9f);
    const float outer = CLAMP(0.f, vr_deadzone_outer.value / 100.f, 0.95f - inner);
    const float len = glm::length(stick);
    if(!(len > inner)) // (NaN too)
    {
        return glm::vec2{0.f};
    }
    const float m = za::min(1.f, (len - inner) / (1.f - inner - outer));
    return stick * (m / len);
}

// The stick's push (moveStick's length, 0..1) on the response curve (vr_move_curve: its power; 1 linear).
[[nodiscard]] float moveCurve(float m, float power)
{
    return za::pow(CLAMP(0.f, m, 1.f), CLAMP(0.25f, power, 4.f));
}

// The speed at a full push: walking cl_forwardspeed, running that times cl_movespeedkey, neither past sv_maxspeed
// (the server's clamp: as the client knows it, the cvar, as in_sdl.c's gamepad; a listen server's own), so the
// whole travel maps onto 0..what the server gives. (Stamina's and crouching's lower clamps are the server's.)
[[nodiscard]] float moveTopSpeed(bool running)
{
    const float cap = sv_maxspeed.value > 0.f ? sv_maxspeed.value : 1e9f;
    return za::min(cl_forwardspeed.value * (running ? cl_movespeedkey.value : 1.f), cap);
}

// Walk into run (vr_move_autorun): how far walking has eased into running (0..1), rising while the push is past
// vr_move_autorun_threshold, falling under it.
constexpr float autorunRiseSeconds = 0.3f;
constexpr float autorunFallSeconds = 0.2f;
float autorunBlend = 0.f;

[[nodiscard]] bool autorunOn(bool running)
{
    return vr_move_autorun.value != 0.f && vr_move_legacy.value == 0.f && !running && cl_alwaysrun.value == 0.f;
}

// The least speed a push past the deadzone asks, on the ground (vr_move_min_speed): Quake's ground friction (under
// sv_stopspeed it takes sv_stopspeed * sv_friction a second) beats the acceleration (sv_accelerate times the speed
// asked, a second) under sv_stopspeed * sv_friction / sv_accelerate, 40: asked less, you stand (or creep a few units a
// second). -1 (the default) that and a tenth more, 0 none (from 0), more that. In water and the air, none (no such floor).
[[nodiscard]] float moveMinSpeed()
{
    if(!cl.onground || cl.inwater)
    {
        return 0.f;
    }
    if(vr_move_min_speed.value >= 0.f)
    {
        return vr_move_min_speed.value;
    }
    return sv_accelerate.value > 0.f ? 1.1f * sv_stopspeed.value * sv_friction.value / sv_accelerate.value : 0.f;
}

// A top speed on the curve (c, 0..1), from the least speed (floor) up: a push past the deadzone always moves you.
[[nodiscard]] float moveRamp(float top, float c, float floor)
{
    return c > 0.f && top > floor ? floor + (top - floor) * c : top * c;
}

// The stick's speed at push m (moveStick's length): running or walking, the top speed on the curve (from floor, the
// least, up: moveMinSpeed); walking with vr_move_autorun, the walk spread over 0..the threshold and eased (blend 0..1)
// into the run's.
[[nodiscard]] float moveSpeed(float m, bool running, float blend, float power, float floor = 0.f)
{
    if(!autorunOn(running))
    {
        return moveRamp(moveTopSpeed(running), moveCurve(m, power), floor);
    }
    const float threshold = CLAMP(0.1f, vr_move_autorun_threshold.value, 1.f);
    const float walk = moveRamp(moveTopSpeed(false), moveCurve(za::min(1.f, m / threshold), power), floor);
    const float run = moveRamp(moveTopSpeed(true), moveCurve(m, power), floor);
    return walk + (za::max(run, walk) - walk) * CLAMP(0.f, blend, 1.f);
}

// The stick's horizontal speed last asked of the server (VR_AdjustMove; the test and vr_debug_stick).
float lastStickSpeed = 0.f;

void stickKey(StickKey& k, float value, bool menu)
{
    constexpr double repeatDelay = 0.4;
    constexpr double repeatInterval = 0.12;

    if(!k.down && value > 0.7f)
    {
        k.down = true;
        k.nextRepeat = realtime + repeatDelay;
        Key_Event(k.key, true);
    }
    else if(k.down && value < 0.5f)
    {
        k.down = false;
        Key_Event(k.key, false);
    }
    else if(k.down && menu && realtime >= k.nextRepeat)
    {
        k.nextRepeat = realtime + repeatInterval;
        Key_Event(k.key, true);
    }
}

void turn(float x)
{
    if(!vr_enable_joystick_turn.value)
    {
        return;
    }

    // Snap by vr_snap_turn degrees, or turn smoothly at vr_turn_speed.
    if(vr_snap_turn.value > 0.f)
    {
        if(za::fabs(x) < 0.3f)
        {
            snapTurnArmed = true;
        }
        else if(snapTurnArmed && za::fabs(x) > 0.7f)
        {
            snapTurnArmed = false;
            hands::stickTurn(x > 0.f ? -vr_snap_turn.value : vr_snap_turn.value, true);
            comfortMotion.snapPulse = 0.3f;
        }
    }
    else if(const float v = deadzone(x); v != 0.f)
    {
        comfortMotion.turn = za::min(1.f, za::fabs(v));
        // (In slow motion at its real-time speed with vr_timescale_turn_realtime or Sandevistan.)
        hands::stickTurn(-v * static_cast<float>(host_frametime) * timescale::turnSpeedup() * 100.f * vr_turn_speed.value, false);
    }
}

// How fast an enemy's shove slides you now (QC vr_enemyshove.qc .vr_eshove_speed; 0 .. 1: a run's 320 units/s and
// more), the local player's on this machine's server (a coop guest's own slide is the host's to read: none here).
[[nodiscard]] float shoveSlide()
{
    edict_t* player = qvr::progs::playerEdict();
    if(player == nullptr)
    {
        return 0.f;
    }
    const float speed = qvr::progs::fieldFloatOr(player, qvr::progs::fields().vr_eshove_speed, 0.f);
    return za::min(1.f, za::max(0.f, speed / 320.f));
}

// Once a frame, after the sticks: the comfort vignette's target (moving: the stick's push, or an enemy's shove sliding
// you: ROUND22.md, "The vore shoves you"; turning: smooth turning's, or a snap's pulse, at once), eased in quickly and
// out more slowly.
void updateComfort()
{
    ComfortMotion& c = comfortMotion;
    const int mode = static_cast<int>(vr_comfort_vignette.value);
    const float dt = static_cast<float>(CLAMP(0.0, host_frametime, 0.1));
    const float moving = mode == 1 || mode == 2 ? za::min(1.f, za::max(glm::length(moveAxes), shoveSlide())) : 0.f;
    const float turning = mode == 1 || mode == 3 ? za::max(c.turn, c.snapPulse > 0.f ? 1.f : 0.f) : 0.f;
    const float target = key_dest == key_game ? za::max(moving, turning) : 0.f;
    const float tau = target > c.amount ? 0.08f : 0.25f;
    c.amount += (target - c.amount) * za::min(1.f, dt / tau);
    if(turning > 0.f && c.snapPulse > 0.f)
    {
        c.amount = za::max(c.amount, target); // a snap is at once: so is its vignette
    }
    c.snapPulse = za::max(0.f, c.snapPulse - dt);
    c.turn = 0.f;
}

void runHaptics()
{
    for(auto it = pendingHaptics.begin(); it != pendingHaptics.end();)
    {
        if(realtime >= it->time)
        {
            haptics::pulse(it->hand, it->seconds, it->amplitude, it->frequency);
            it = pendingHaptics.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// Bump when quakevr/vr_bindings.cfg changes in a way existing configs should pick up.
constexpr int bindingsVersion = 3;

void checkBindings_f()
{
    if(vr_bindings_version.value >= bindingsVersion)
    {
        return;
    }

    Con_Printf("Quake VR: setting the controller bindings (vr_bindings.cfg)\n");
    Cbuf_InsertText("exec vr_bindings.cfg\n");
    Cvar_SetValueQuick(&vr_bindings_version, static_cast<float>(bindingsVersion));
}

// vr_bindings_reset (Debug > Logging > Restore Controller Bindings): vr_bindings.cfg again, whatever the version.
void resetBindings_f()
{
    Con_Printf("Quake VR: setting the controller bindings (vr_bindings.cfg)\n");
    Cbuf_InsertText("exec vr_bindings.cfg\n");
    Cvar_SetValueQuick(&vr_bindings_version, static_cast<float>(bindingsVersion));
}

// guardBindings' last repair, for qvr_openxr.txt (describeBindings): its log is not open yet at vr_startgame.
char bindingsRepair[256];

[[nodiscard]] bool bindsGrab(int key)
{
    const char* b = keybindings[key];
    return b && q_strcasestr(b, "grab");
}

// ---- vr_stick_curve_test [x y z]: the stick's locomotion response (ROUND22.md "Thumbstick locomotion response"). First
// the response itself (moveStick, moveSpeed) over a sweep of pushes and angles: a table of push -> speed, linear and
// curved, and the checks (monotonic, nothing inside the deadzone, full at 95%, about half at 65% with 1.6, running's
// top sv_maxspeed, diagonals as straight). Then, in a map (a listen server), each case below driven on the moving stick
// from x y z (default: where the player stands; room round it for 200 units): the speed asked and the player's after
// it settles. Ends "stick curve test: N passed, M failed". Sets the speeds it measures against (cl_forwardspeed 200,
// cl_movespeedkey 2, sv_maxspeed 320), the head steering, no crouch or stamina slowing; puts them back after.
struct StickCase
{
    float push;  // the stick's raw length
    float angle; // degrees from straight ahead (clockwise)
    bool run;
    bool autorun;
    float curve;
    int mode{1};  // vr_movement_mode (1 the head steers; 0, 2, 3 a hand)
    int where{0}; // 0 on the ground, 1 swimming (the test hall's pool), 2 noclip
};

constexpr StickCase stickCases[] = {
    {0.05f, 0.f, false, false, 1.6f},  {0.12f, 0.f, false, false, 1.6f}, {0.3f, 0.f, false, false, 1.6f},  {0.5f, 0.f, false, false, 1.6f},
    {0.65f, 0.f, false, false, 1.6f},  {0.8f, 0.f, false, false, 1.6f},  {0.95f, 0.f, false, false, 1.6f},
    {1.f, 0.f, false, false, 1.6f},    {0.65f, 45.f, false, false, 1.6f}, {1.f, 45.f, false, false, 1.6f},
    {1.2f, 45.f, false, false, 1.6f},  {1.f, 120.f, false, false, 1.6f}, {0.65f, 0.f, false, false, 1.f},
    {0.65f, 0.f, true, false, 1.6f},   {0.9f, 0.f, true, false, 1.6f},   {0.95f, 0.f, true, false, 1.6f},
    {1.f, 0.f, true, false, 1.6f},     {1.f, 45.f, true, false, 1.6f},   {1.2f, 30.f, true, false, 1.6f},
    {0.8f, 0.f, false, true, 1.6f},    {0.85f, 0.f, false, true, 1.6f},  {1.f, 0.f, false, true, 1.6f},
    {1.f, 45.f, false, true, 1.6f},
    // the hands steering, swimming and noclip: the same push still moves you (they take the same forward and side)
    {1.f, 0.f, false, false, 1.6f, 0}, {1.f, 0.f, false, false, 1.6f, 2}, {1.f, 0.f, true, false, 1.6f, 3},
    {0.65f, 0.f, false, false, 1.6f, 0}, {1.f, 0.f, false, false, 1.6f, 1, 1}, {0.65f, 0.f, true, false, 1.6f, 1, 1},
    {1.f, 0.f, true, false, 1.6f, 1, 2}, {0.65f, 0.f, false, false, 1.6f, 1, 2},
};
constexpr int stickCaseCount = static_cast<int>(sizeof(stickCases) / sizeof(stickCases[0]));
constexpr int stickSettleSpan = 12;      // frames the speed must hold still over
constexpr int stickGiveUpFrames = 900;   // a case not settled by then: FAIL "timed out" (a safety net)
constexpr float stickTestWalk = 200.f;   // the speeds the test sets and measures against
constexpr float stickTestRun = 320.f;

struct StickTest
{
    bool active{false};
    int index{0};
    int frames{0};
    glm::vec3 start{0.f};
    glm::vec2 stick{0.f};
    float history[stickSettleSpan]{};
    float moved[stickCaseCount]{};
    int passed{0};
    int failed{0};
    // the settings it changes, put back after
    float alwaysrun{0.f}, speedkey{0.f}, forward{0.f}, maxspeed{0.f}, crouch{0.f}, stamina{0.f}, mode{0.f};
    float curve{0.f}, autorun{0.f}, threshold{0.f}, inner{0.f}, outer{0.f}, least{0.f};
};
StickTest stickTest;
int stickDebugFrames = 0; // vr_debug_stick: frames since its last line

void stickCheck(bool ok, const char* what)
{
    (ok ? stickTest.passed : stickTest.failed)++;
    if(!ok)
    {
        Con_Printf("stick curve: FAIL %s\n", what);
    }
}

// What a case should ask on the ground, worked out apart from moveSpeed: the push past 10% to 95% onto 0..1, on the
// curve, from the least speed (44: 1.1 sv_stopspeed sv_friction / sv_accelerate) to 200 walking or 320 running;
// walking into running (settled: run past the 0.9 threshold, else the walk over 0..0.9).
[[nodiscard]] float stickExpected(const StickCase& c)
{
    const float m = CLAMP(0.f, (c.push - 0.1f) / 0.85f, 1.f);
    if(m == 0.f)
    {
        return 0.f;
    }
    const float least = 1.1f * sv_stopspeed.value * sv_friction.value / sv_accelerate.value;
    if(c.autorun && !c.run)
    {
        return m >= 0.9f ? za::max(least + (stickTestRun - least) * za::pow(m, c.curve), stickTestWalk)
                         : least + (stickTestWalk - least) * za::pow(za::min(1.f, m / 0.9f), c.curve);
    }
    return least + ((c.run ? stickTestRun : stickTestWalk) - least) * za::pow(m, c.curve);
}

[[nodiscard]] edict_t* stickPlayer()
{
    return qvr::progs::localPlayer();
}

[[nodiscard]] float stickPlayerSpeed()
{
    const edict_t* p = stickPlayer();
    return p ? za::sqrt(p->v.velocity[0] * p->v.velocity[0] + p->v.velocity[1] * p->v.velocity[1]) : 0.f;
}

void stickTestRestore()
{
    StickTest& t = stickTest;
    Cvar_SetValueQuick(&cl_alwaysrun, t.alwaysrun);
    Cvar_SetValueQuick(&cl_movespeedkey, t.speedkey);
    Cvar_SetValueQuick(&cl_forwardspeed, t.forward);
    Cvar_SetValueQuick(&sv_maxspeed, t.maxspeed);
    Cvar_SetValueQuick(&vr_crouch_speed, t.crouch);
    Cvar_SetValueQuick(&vr_stamina_speed, t.stamina);
    Cvar_SetValueQuick(&vr_movement_mode, t.mode);
    Cvar_SetValueQuick(&vr_move_curve, t.curve);
    Cvar_SetValueQuick(&vr_move_autorun, t.autorun);
    Cvar_SetValueQuick(&vr_move_autorun_threshold, t.threshold);
    Cvar_SetValueQuick(&vr_deadzone, t.inner);
    Cvar_SetValueQuick(&vr_deadzone_outer, t.outer);
    Cvar_SetValueQuick(&vr_move_min_speed, t.least);
}

void stickTestFinish()
{
    stickTest.active = false;
    stickTest.stick = glm::vec2{0.f};
    stickTestRestore();
    if(edict_t* p = stickPlayer(); p && stickTest.index > 0)
    {
        p->v.movetype = static_cast<float>(MOVETYPE_WALK); // (out of the noclip cases)
    }
    qvr::testkit::verdict("stick curve test", stickTest.passed, stickTest.failed);
}

// The response's own checks and its table (the test's first part; also Debug > Tests > Stick: Curve Table).
void stickTable()
{
    // (the curve itself, from 0; "ground": from the least speed on the ground, moveMinSpeed's default)
    const float least = 1.1f * sv_stopspeed.value * sv_friction.value / za::max(sv_accelerate.value, 1e-3f);
    Con_Printf("stick curve: push | m     | walk lin | walk 1.6 | run lin | run 1.6 | ground walk 1.6 | ground run 1.6\n");
    for(int i = 0; i <= 20; ++i)
    {
        const float r = static_cast<float>(i) * 0.05f;
        const float m = glm::length(moveStick({0.f, r}));
        Con_Printf("stick curve: %4.2f | %5.3f | %8.1f | %8.1f | %7.1f | %7.1f | %15.1f | %14.1f\n", r, m,
                   moveSpeed(m, false, 0.f, 1.f), moveSpeed(m, false, 0.f, 1.6f), moveSpeed(m, true, 0.f, 1.f),
                   moveSpeed(m, true, 0.f, 1.6f), moveSpeed(m, false, 0.f, 1.6f, least), moveSpeed(m, true, 0.f, 1.6f, least));
    }

    char what[160];
    // on the ground: monotonic, and a push just past the deadzone moves you (Quake's friction holds nothing under 40)
    for(const bool run : {false, true})
    {
        float last = 0.f;
        bool monotonic = true, moves = true;
        for(int i = 11; i <= 120; ++i)
        {
            const float v = moveSpeed(glm::length(moveStick({0.f, static_cast<float>(i) * 0.01f})), run, 0.f, 1.6f, least);
            monotonic = monotonic && v >= last - 1e-4f;
            moves = moves && v >= 40.f;
            last = v;
        }
        q_snprintf(what, sizeof(what), "ground %s: monotonic, at least 40 past the deadzone", run ? "run" : "walk");
        stickCheck(monotonic && moves, what);
    }
    for(const float p : {1.f, 1.6f, 3.f})
    {
        for(const bool run : {false, true})
        {
            const float top = run ? stickTestRun : stickTestWalk;
            float last = 0.f;
            bool monotonic = true, dead = true, full = true;
            for(int i = 0; i <= 120; ++i)
            {
                const float r = static_cast<float>(i) * 0.01f;
                const float v = moveSpeed(glm::length(moveStick({0.f, r})), run, 0.f, p);
                monotonic = monotonic && v >= last - 1e-4f;
                last = v;
                dead = dead && (r > 0.1f || v == 0.f);
                full = full && (r < 0.95f || za::fabs(v - top) < 0.01f);
            }
            q_snprintf(what, sizeof(what), "curve %.1f %s: monotonic", p, run ? "run" : "walk");
            stickCheck(monotonic, what);
            q_snprintf(what, sizeof(what), "curve %.1f %s: nothing inside the deadzone", p, run ? "run" : "walk");
            stickCheck(dead, what);
            q_snprintf(what, sizeof(what), "curve %.1f %s: full speed (%.0f) from a 95%% push", p, run ? "run" : "walk", top);
            stickCheck(full, what);
        }
    }
    const float half = moveSpeed(glm::length(moveStick({0.f, 0.65f})), false, 0.f, 1.6f) / stickTestWalk;
    Con_Printf("stick curve: curve 1.6 at a 65%% push: %.3f of the top speed\n", half);
    stickCheck(half > 0.45f && half < 0.55f, "curve 1.6: about half speed at a 65% push");

    // diagonals as straight, and never past a full push
    bool diagonal = true;
    for(const float r : {0.2f, 0.5f, 0.65f, 0.9f, 1.f, 1.3f})
    {
        const float straight = glm::length(moveStick({0.f, r}));
        for(int a = 0; a < 360; a += 15)
        {
            const float rad = static_cast<float>(a) * (3.14159265f / 180.f);
            const float l = glm::length(moveStick({r * glm::sin(rad), r * glm::cos(rad)}));
            diagonal = diagonal && za::fabs(l - straight) < 1e-4f && l <= 1.0001f;
        }
    }
    stickCheck(diagonal, "diagonals as straight pushes, never past 1");

    // walking into running: the walk full at the threshold, the blend never down, never past the run's top
    Cvar_SetValueQuick(&vr_move_autorun, 1.f);
    bool blendUp = true;
    for(const float m : {0.5f, 0.9f, 0.95f, 1.f})
    {
        float last = 0.f;
        for(int b = 0; b <= 10; ++b)
        {
            const float v = moveSpeed(m, false, static_cast<float>(b) * 0.1f, 1.6f);
            blendUp = blendUp && v >= last - 1e-4f && v <= stickTestRun + 0.01f;
            last = v;
        }
    }
    stickCheck(blendUp, "walk into run: the blend only adds, up to the run's top");
    stickCheck(za::fabs(moveSpeed(0.9f, false, 0.f, 1.6f) - stickTestWalk) < 0.01f, "walk into run: full walk at the threshold");
    stickCheck(za::fabs(moveSpeed(1.f, false, 1.f, 1.6f) - stickTestRun) < 0.01f, "walk into run: full run eased in");
    Cvar_SetValueQuick(&vr_move_autorun, 0.f);
}

void stickTestSetCase()
{
    StickTest& t = stickTest;
    const StickCase& c = stickCases[t.index];
    Cvar_SetValueQuick(&cl_alwaysrun, c.run ? 1.f : 0.f);
    Cvar_SetValueQuick(&vr_move_autorun, c.autorun ? 1.f : 0.f);
    Cvar_SetValueQuick(&vr_move_curve, c.curve);
    Cvar_SetValueQuick(&vr_movement_mode, static_cast<float>(c.mode));
    const float rad = c.angle * (3.14159265f / 180.f);
    t.stick = {c.push * glm::sin(rad), c.push * glm::cos(rad)};
    t.frames = 0;
    if(edict_t* p = stickPlayer())
    {
        // (swimming: under the test hall's pool's water, its middle)
        const glm::vec3 at = c.where == 1 ? glm::vec3{384.f, -256.f, -64.f} : t.start;
        p->v.movetype = static_cast<float>(c.where == 2 ? MOVETYPE_NOCLIP : MOVETYPE_WALK);
        VectorCopy(at, p->v.origin);
        VectorCopy(vec3_origin, p->v.velocity);
        progs::ServerVm serverVm; // (the input's frame: the client's VM, or none, is in)
        SV_LinkEdict(p, false);
    }
}

void stickTest_f()
{
    StickTest& t = stickTest;
    if(t.active)
    {
        Con_Printf("stick curve: already running\n");
        return;
    }
    t = StickTest{};
    t.alwaysrun = cl_alwaysrun.value;
    t.speedkey = cl_movespeedkey.value;
    t.forward = cl_forwardspeed.value;
    t.maxspeed = sv_maxspeed.value;
    t.crouch = vr_crouch_speed.value;
    t.stamina = vr_stamina_speed.value;
    t.mode = vr_movement_mode.value;
    t.curve = vr_move_curve.value;
    t.autorun = vr_move_autorun.value;
    t.threshold = vr_move_autorun_threshold.value;
    t.inner = vr_deadzone.value;
    t.outer = vr_deadzone_outer.value;
    t.least = vr_move_min_speed.value;
    Cvar_SetValueQuick(&cl_alwaysrun, 0.f);
    Cvar_SetValueQuick(&cl_movespeedkey, 2.f);
    Cvar_SetValueQuick(&cl_forwardspeed, stickTestWalk);
    Cvar_SetValueQuick(&sv_maxspeed, stickTestRun);
    Cvar_SetValueQuick(&vr_crouch_speed, 0.f);
    Cvar_SetValueQuick(&vr_stamina_speed, 0.f);
    Cvar_SetValueQuick(&vr_movement_mode, 1.f);
    Cvar_SetValueQuick(&vr_move_autorun, 0.f);
    Cvar_SetValueQuick(&vr_move_autorun_threshold, 0.9f);
    Cvar_SetValueQuick(&vr_deadzone, 10.f);
    Cvar_SetValueQuick(&vr_deadzone_outer, 5.f);
    Cvar_SetValueQuick(&vr_move_min_speed, -1.f);

    stickTable();
    const bool table = Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "table");
    edict_t* p = stickPlayer();
    if(table || !p || !vrActive())
    {
        if(!table)
        {
            Con_Printf("stick curve: no player in VR (a map, in VR): the response only\n");
        }
        stickTestFinish();
        return;
    }
    if(Cmd_Argc() >= 4)
    {
        t.start = {static_cast<float>(atof(Cmd_Argv(1))), static_cast<float>(atof(Cmd_Argv(2))),
                   static_cast<float>(atof(Cmd_Argv(3)))};
    }
    else
    {
        t.start = {p->v.origin[0], p->v.origin[1], p->v.origin[2]};
    }
    if(p->v.movetype == static_cast<float>(MOVETYPE_NOCLIP))
    {
        p->v.movetype = static_cast<float>(MOVETYPE_WALK);
    }
    t.active = true;
    t.index = 0;
    stickTestSetCase();
}

// Each frame (update): a case's speed settled (held still over stickSettleSpan frames, at least 48 frames in), its line
// and checks, then the next case from the start.
void stickTestTick()
{
    StickTest& t = stickTest;
    if(!t.active)
    {
        return;
    }
    if(!stickPlayer())
    {
        Con_Printf("stick curve: FAIL the player is gone\n");
        ++t.failed;
        stickTestFinish();
        return;
    }
    const float speed = stickPlayerSpeed();
    const float before = t.history[t.frames % stickSettleSpan];
    t.history[t.frames % stickSettleSpan] = speed;
    ++t.frames;
    const StickCase& c = stickCases[t.index];
    const bool settled = t.frames >= 48 && za::fabs(speed - before) < 0.25f;
    if(!settled && t.frames < stickGiveUpFrames)
    {
        return;
    }

    const float expected = stickExpected(c);
    char what[200];
    static constexpr const char* places[] = {"", " swimming", " noclip"};
    q_snprintf(what, sizeof(what), "push %.2f at %.0f deg %s%s curve %.1f%s%s", c.push, c.angle, c.run ? "run" : "walk",
               c.autorun ? " autorun" : "", c.curve, c.mode != 1 ? va(" mode %d", c.mode) : "", places[c.where]);
    Con_Printf("stick curve: %s: asked %.1f (expected %.1f), moved %.1f%s\n", what, lastStickSpeed, expected, speed,
               settled ? "" : " (timed out)");
    char check[260];
    q_snprintf(check, sizeof(check), "%s: timed out", what);
    stickCheck(settled, check);
    if(c.mode == 1 && c.where == 0)
    {
        q_snprintf(check, sizeof(check), "%s: asked %.1f, expected %.1f", what, lastStickSpeed, expected);
        stickCheck(za::fabs(lastStickSpeed - expected) < 0.05f, check);
        q_snprintf(check, sizeof(check), "%s: moved %.1f, asked %.1f", what, speed, lastStickSpeed);
        stickCheck(za::fabs(speed - lastStickSpeed) <= 1.f + 0.01f * lastStickSpeed, check);
    }
    else
    {
        // a hand steering (where it points: no faster than the head), in water (slower: Quake's 0.7 and the swimming),
        // noclip (no floor: from 0): it moves, never past the top
        const float top = c.run ? stickTestRun : stickTestWalk;
        q_snprintf(check, sizeof(check), "%s: asked %.1f, moved %.1f (top %.0f)", what, lastStickSpeed, speed, top);
        // (immersive swimming, vr_swim 1: the stick at vr_swim_stick_speed, 0.25, of Quake's 0.7)
        const float slowest = c.where == 1 ? 0.1f : 0.3f;
        stickCheck(lastStickSpeed <= expected + 0.05f && speed > slowest * lastStickSpeed && lastStickSpeed > 0.f &&
                       speed <= lastStickSpeed * 1.01f + 1.f,
                   check);
    }
    t.moved[t.index] = speed;
    // the same push at an angle as straight ahead
    for(int i = 0; i < t.index; ++i)
    {
        const StickCase& o = stickCases[i];
        if(o.push == c.push && o.run == c.run && o.autorun == c.autorun && o.curve == c.curve && o.angle == 0.f &&
           c.angle != 0.f && o.mode == c.mode && o.where == c.where)
        {
            q_snprintf(check, sizeof(check), "%s: moved %.1f, straight %.1f", what, speed, t.moved[i]);
            stickCheck(za::fabs(speed - t.moved[i]) <= 1.f, check);
        }
    }

    if(++t.index >= stickCaseCount)
    {
        stickTestFinish();
        return;
    }
    stickTestSetCase();
}

// vr_debug_stick 1: twice a second, the moving stick's push (raw length, rescaled) and the speed asked and moved.
void stickDebug(glm::vec2 raw)
{
    if(vr_debug_stick.value == 0.f || ++stickDebugFrames < 36)
    {
        return;
    }
    stickDebugFrames = 0;
    Con_Printf("stick: push %.2f -> %.3f, asked %.0f (top %.0f walk, %.0f run), moved %.0f\n", glm::length(raw),
               glm::length(moveAxes), lastStickSpeed, moveTopSpeed(false), moveTopSpeed(true), stickPlayerSpeed());
}

} // namespace

namespace qvr::input
{

float stickAxis(float v)
{
    return deadzone(v);
}

void init()
{
    Cmd_AddCommand("vr_checkbindings", checkBindings_f);
    Cmd_AddCommand("vr_bindings_reset", resetBindings_f);
    Cmd_AddCommand("vr_bindings_check", guardBindings); // what vr_startgame runs (tests)
    Cmd_AddCommand("vr_stick_curve_test", stickTest_f);
}

// 1.0.1 (ROUND22.md "Controls as if another controller": a player's left trigger jumped, his grips grabbed nothing, A
// did nothing: Ironwail's own gamepad bindings, LTRIGGER +jump, LSHOULDER/RSHOULDER "impulse 12"/"impulse 10", from a
// config run after Quake VR's). A controller with a grab on none of its keys is never a VR player's choice (no weapon,
// ladder or ledge can be taken in that hand; a config that rebinds only the left grip leaves the right one grabbing):
// the VR bindings again. Rebinding a grip to another grab command (an alias with "grab" in its name) is left alone.
void guardBindings()
{
    if(vr_bindings_guard.value == 0.f)
    {
        return;
    }
    bool grabs[HAND_COUNT] = {};
    for(const ButtonKeys& b : buttonKeys)
    {
        for(int h = 0; h < HAND_COUNT; ++h)
        {
            grabs[h] = grabs[h] || bindsGrab(b.key[h]);
        }
    }
    if(grabs[0] && grabs[1])
    {
        return;
    }

    const auto bound = [](int key) { return keybindings[key] ? keybindings[key] : ""; };
    q_snprintf(bindingsRepair, sizeof(bindingsRepair),
        "no grab on %s controller's keys (LTRIGGER \"%s\", LSHOULDER \"%s\", RSHOULDER \"%s\", ABUTTON \"%s\")",
        !grabs[0] && !grabs[1] ? "either" : !grabs[0] ? "the left" : "the right", bound(K_LTRIGGER), bound(K_LSHOULDER),
        bound(K_RSHOULDER), bound(K_ABUTTON));
    Con_Warning("Quake VR: %s: another config's gamepad bindings. Setting the controller bindings again (vr_bindings.cfg; "
                "vr_bindings_guard 0: never)\n",
        bindingsRepair);
    // Where they may have come from: the loose configs on the search path that ran at the start (a pak's are Quake's own;
    // only the game folder's own autoexec.cfg and config.cfg, vr_foreign_configs 0, and its own saved config).
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        for(const char* name : {"config.cfg", "autoexec.cfg", CONFIG_NAME})
        {
            const bool ran = (Cmd_ForeignConfigsAllowed() && strcmp(name, CONFIG_NAME) != 0) ||
                             COM_SamePath(s->filename, com_gamedir);
            if(!s->pack && ran && Sys_FileType(va("%s/%s", s->filename, name)) == FS_ENT_FILE)
            {
                Con_Printf("  %s/%s\n", s->filename, name);
            }
        }
    }
    Cbuf_InsertText("exec vr_bindings.cfg\n");
}

void describeBindings(char* out, size_t size)
{
    const auto bound = [](int key) { return keybindings[key] ? keybindings[key] : ""; };
    q_snprintf(out, size,
        "LTRIGGER \"%s\", RTRIGGER \"%s\", LSHOULDER \"%s\", RSHOULDER \"%s\", XBUTTON \"%s\", ABUTTON \"%s\", YBUTTON \"%s\", "
        "BBUTTON \"%s\"%s%s",
        bound(K_LTRIGGER), bound(K_RTRIGGER), bound(K_LSHOULDER), bound(K_RSHOULDER), bound(K_XBUTTON), bound(K_ABUTTON),
        bound(K_YBUTTON), bound(K_BBUTTON), bindingsRepair[0] ? "; set again at the start: " : "", bindingsRepair);
}

namespace
{
const InputState released; // (update: without VR, everything is released)
// update's phases: a hand's button changed: whatever takes it first (a menu gesture, a tool, a mode), else its key.
void buttonChanged(const InputState& in, int h, const ButtonKeys& b, bool now)
{
    // A menu gesture's button is the menu's alone (vr_menu_button: both stick clicks, Y).
    if(menuGestureButton(in, h, b.button, now))
    {
        logButton(h, b, now, b.key[h], "the menu gesture (vr_menu_button)");
        return;
    }
    // Holster Setup takes every button while it runs (vr_holstersetup.cpp): nothing reaches the game.
    if(holstersetup::button(h, posingButton(b.button), now))
    {
        logButton(h, b, now, b.key[h], "Holster Setup");
        return;
    }
    // So does Edit Blade Shape (vr_bladeedit.cpp).
    if(bladeedit::button(h, posingButton(b.button), now))
    {
        logButton(h, b, now, b.key[h], "Edit Blade Shape");
        return;
    }
    // And the Pose Studio (vr_posestudio.cpp): the grips, triggers, B/Y (not A/X or the sticks: you walk).
    if(posestudio::button(h, posingButton(b.button), now))
    {
        logButton(h, b, now, b.key[h], "the Pose Studio");
        return;
    }
    // The weapon posing mode takes the buttons pressed while it runs (vr_posing.cpp).
    if(posing::button(h, posingButton(b.button), now))
    {
        logButton(h, b, now, b.key[h], "the weapon posing mode");
        return;
    }
    // The Survival Arena's perk cards: a trigger with its laser on their panel picks (vr_arenacards.cpp).
    if(arenacards::button(h, posingButton(b.button), now))
    {
        logButton(h, b, now, b.key[h], "the perk cards");
        return;
    }
    // The toolgun's hand: its trigger, B/Y (its menu) and X/A (the sticks); the other trigger while its physgun
    // holds something (vr_toolgun.cpp).
    if(toolgun::button(h, posingButton(b.button), now))
    {
        logButton(h, b, now, b.key[h], "the toolgun");
        return;
    }
    // The off hand's upper button at the mouth records a voice note instead (not while that hand holds
    // the flashlight or is at it on the head: its Y turns it round in the fist, clips it on the head or
    // takes it off; a note's release always ends it).
    if(h == HAND_OFF && b.button == &HandInput::secondary && (!now || !flashlight::wantsSecondary(HAND_OFF)) &&
        voicenotes::offhandButton(now))
    {
        logButton(h, b, now, b.key[h], "a voice note");
        return;
    }
    // With the motion recorder armed, its record button (vr_motion_button) starts and ends a take instead.
    if(motion::button(h, b.button, now))
    {
        logButton(h, b, now, b.key[h], "the motion recorder");
        return;
    }
    // At a holster with a weapon in it (or one bound), the magnetic holsters' bind button binds it to the
    // holster or unbinds it (vr_magholster_button; vr_magholster.cpp): its key is never sent.
    if(magholster::button(h, b.button, now))
    {
        logButton(h, b, now, b.key[h], "the magnetic holsters");
        return;
    }
    // A stick press chosen for the seated crouch toggle (vr_seated_crouch_button) does only that.
    if(b.button == &HandInput::stickClick && seated::stickPress(h, now))
    {
        logButton(h, b, now, b.key[h], "the crouch toggle");
        return;
    }
    // A stick press chosen to start bullet time (vr_bullettime_trigger) does only that in the game; its
    // bound key (LTHUMB: +speed, RTHUMB: +reloadmain by default) is never sent.
    if(b.button == &HandInput::stickClick && bullettime::stickPress(h, now))
    {
        logButton(h, b, now, b.key[h], "bullet time");
        return;
    }
    // A grip in a holster zone switched to dragging takes hold of it (vr_holster_zone_<kind>_drag): never the
    // game's (no gun drawn or put away there).
    if(b.button == &HandInput::grip && zones::dragGrip(h, now))
    {
        logButton(h, b, now, b.key[h], "a holster zone's drag");
        return;
    }
    // An empty hand at the cord's handle of the chainsaw in the other takes the cord (vr_chainsaw.cpp).
    if(b.button == &HandInput::grip && chainsaw::grip(h, now))
    {
        return;
    }
    // A hand at the chest flashlight switches it (trigger) or takes it (grip) instead; the
    // upper button (B/Y) clips it on a gun or takes it off (the flashlight's grip then).
    if(b.button == &HandInput::trigger || b.button == &HandInput::grip || b.button == &HandInput::secondary)
    {
        const auto fb = b.button == &HandInput::trigger ? flashlight::Button::Trigger
                        : b.button == &HandInput::grip  ? flashlight::Button::Grip
                                                         : flashlight::Button::Secondary;
        if(flashlight::button(h, fb, now))
        {
            for(int g = 0; g < 2; g++) // (either hand: a release can pass the torch to the other one)
            {
                if(flashlight::tookGrip(g))
                {
                    keyEvent(buttonKeys[1].key[g], false); // the grip's key
                }
            }
            logButton(h, b, now, b.key[h], "the flashlight");
            return;
        }
    }
    // A grip on the menu panel's frame takes hold of it (vr_menu_grab).
    if(b.button == &HandInput::grip && menuui::gripButton(h, now))
    {
        logButton(h, b, now, b.key[h], "the menu panel's grab");
        return;
    }
    // A trigger pointing at the menu is its mouse button.
    const int key = b.button == &HandInput::trigger ? menuui::triggerKey(h, now, b.key[h]) : b.key[h];
    logButton(h, b, now, key, nullptr);
    keyEvent(key, now);
    if(b.button == &HandInput::secondary)
    {
        secondaryDown[h] = now;
    }
    else if(b.button == &HandInput::primary)
    {
        primaryDown[h] = now;
    }
    if(now && key_dest == key_menu && !vr_disablehaptics.value)
    {
        // A click under the finger, as the old engine gave in menus.
        pendingHaptics.pushBack({realtime, h, 0.02f, 150.f, 0.3f});
    }
}

// The sticks: the menu's keys, a tool's or mode's, or walking and turning.
void routeSticks(const InputState& in, const HandInput& off, const HandInput& main, bool active)
{
    const bool menu = key_dest != key_game;
    if(menu)
    {
        // The off hand's stick navigates and changes values (the arrow keys). The main hand's only
        // scrolls (on a page with a scrollbar) or moves the selection up and down: its left and
        // right do nothing, so that navigating never changes a setting by accident (round 20).
        if(glm::length(off.stick) > 0.3f || glm::length(main.stick) > 0.3f)
        {
            menuui::sticksPushed(); // (a grip held on the panel is the fine modifier, not its grab)
        }
        const bool scrolls = menuui::scrollStick(main.stick.y);
        if(scrolls && za::fabs(main.stick.y) > 0.3f)
        {
            mainStickScrolls = true;
        }
        else if(za::fabs(main.stick.x) < 0.3f && za::fabs(main.stick.y) < 0.3f)
        {
            mainStickScrolls = false;
        }
        glm::vec2 stick = off.stick;
        if(!scrolls && !mainStickScrolls)
        {
            stick.y += main.stick.y;
        }
        stickKey(stickKeys[0], stick.y, true);
        stickKey(stickKeys[1], -stick.y, true);
        stickKey(stickKeys[2], -stick.x, true);
        stickKey(stickKeys[3], stick.x, true);
        moveAxes = glm::vec2{0.f};
    }
    else if(poselive::sticksTaken())
    {
        // The Pose Studio's Live mode paused: either stick left or right scrubs its tape (no walking, no turning).
        for(StickKey& k : stickKeys)
        {
            stickKey(k, 0.f, false);
        }
        moveAxes = glm::vec2{0.f};
        poselive::sticks(in.hands[HAND_OFF].stick, in.hands[HAND_MAIN].stick);
    }
    else if(toolgun::sticksTaken())
    {
        // The toolgun's physgun holding something, or its X/A held: the sticks move and turn what it holds or places (no
        // walking, no turning).
        for(StickKey& k : stickKeys)
        {
            stickKey(k, 0.f, false);
        }
        moveAxes = glm::vec2{0.f};
        toolgun::sticks(in.hands[HAND_OFF].stick, in.hands[HAND_MAIN].stick);
    }
    else if(holstersetup::active())
    {
        // Holster Setup: the player stands still (the game paused); the stick of a hand holding a holster resizes it.
        for(StickKey& k : stickKeys)
        {
            stickKey(k, 0.f, false);
        }
        moveAxes = glm::vec2{0.f};
        holstersetup::sticks(in.hands[HAND_OFF].stick, in.hands[HAND_MAIN].stick);
    }
    else if(bladeedit::active())
    {
        // Edit Blade Shape: the player stands still while the hands shape the blade.
        for(StickKey& k : stickKeys)
        {
            stickKey(k, 0.f, false);
        }
        moveAxes = glm::vec2{0.f};
        bladeedit::sticks(in.hands[HAND_OFF].stick, in.hands[HAND_MAIN].stick);
    }
    else if(posing::active())
    {
        // Posing: the player stands still; the confirming hand's stick turns the floating weapon.
        for(StickKey& k : stickKeys)
        {
            stickKey(k, 0.f, false);
        }
        moveAxes = glm::vec2{0.f};
        posing::sticks(in.hands[HAND_OFF].stick, in.hands[HAND_MAIN].stick);
    }
    else
    {
        stickKey(stickKeys[0], main.stick.y, false);
        stickKey(stickKeys[1], -main.stick.y, false);
        stickKey(stickKeys[2], 0.f, false);
        stickKey(stickKeys[3], 0.f, false);
        // (vr_stick_curve_test drives the moving stick while it runs.)
        const glm::vec2 moving = stickTest.active ? stickTest.stick : off.stick;
        moveAxes = vr_move_legacy.value != 0.f ? glm::vec2{deadzone(moving.x), deadzone(moving.y)} : moveStick(moving);
        stickDebug(off.stick);
        if(active)
        {
            turn(main.stick.x);
        }
    }
}

} // namespace

const char* menuButtonName()
{
    return vr_menu_button.value >= 1.f ? "menu button or both sticks" : "menu button";
}

void update(const InputState& tracked)
{
    // Without VR, everything is released (keys held when the session ends must come up).
    const bool active = vrActive();
    const InputState& raw = active ? tracked : released;
    // Doom on the wrist gadget with the focus takes the gadget hand's stick and buttons (its menu button stays Quake's):
    // Quake sees them let go meanwhile (vr_doom.hpp).
    InputState masked;
    const bool doomTakes = doom::takesOffHand(raw);
    if(doomTakes)
    {
        masked = raw;
        const int g = hands::gadgetHand();
        HandInput none;
        none.menu = raw.hands[g].menu;
        masked.hands[g] = none;
    }
    const InputState& in = doomTakes ? masked : raw;

    // The sticks by what they do: the moving one (vr_stick_swap: the right), the turning one.
    const HandInput& off = in.hands[hands::moveHand()];
    const HandInput& main = in.hands[1 - hands::moveHand()];

    // Where the hands point at the menu, before the trigger clicks there.
    menuui::update(hands::current());

    // vr_menu_button's ways to the menu: Y or B held (the stick chord and Y pressed: with the buttons, below).
    for(int h = 0; h < HAND_COUNT; h++)
    {
        menuGestures.press[h] = false;
    }
    menuHold(secondaryDown);

    for(int h = 0; h < HAND_COUNT; h++)
    {
        for(const ButtonKeys& b : buttonKeys)
        {
            const bool now = in.hands[h].*b.button;
            if(now != previous.hands[h].*b.button)
            {
                buttonChanged(in, h, b, now);
            }
        }

        // The menu button, or a gesture standing for it (a press for one frame).
        const bool menuNow = in.hands[h].menu || menuGestures.press[h];
        const bool menuBefore = menuGestures.menuWas[h];
        menuGestures.menuWas[h] = menuNow;
        if(menuNow == menuBefore ||
            (!holstersetup::button(h, posing::Button::Menu, menuNow) && !bladeedit::button(h, posing::Button::Menu, menuNow) &&
                !posestudio::button(h, posing::Button::Menu, menuNow) &&
                !posing::button(h, posing::Button::Menu, menuNow)))
        {
            menuButton(h, menuNow, menuBefore, menuGestures.press[h] ? menuGestures.source[h] : "menu button");
        }
    }

    // A grip pressed on the way to the flashlight takes it as the hand comes to rest there: the game saw the press,
    // it sees the grip let go now (the release is the flashlight's).
    flashlight::lateGrips();
    zones::dragFrame(); // (a holster zone held by a hand follows it)
    for(int g = 0; g < HAND_COUNT; g++)
    {
        if(flashlight::tookGrip(g))
        {
            keyEvent(buttonKeys[1].key[g], false);
        }
    }

    routeSticks(in, off, main, active);

    updateComfort();
    stickTestTick();
    previous = in;
    if(!playbackCommands.empty())
    {
        Cbuf_InsertText(playbackCommands.cStr());
        playbackCommands.clear();
    }
    runHaptics();
}

float comfortVignette()
{
    const float strength = CLAMP(0.f, vr_comfort_vignette_strength.value, 1.f);
    return vr_comfort_vignette.value != 0.f ? strength * comfortMotion.amount : 0.f;
}

bool secondaryHeld(int hand)
{
    return hand >= 0 && hand < HAND_COUNT && secondaryDown[hand] && key_dest == key_game;
}

bool primaryHeld(int hand)
{
    return hand >= 0 && hand < HAND_COUNT && primaryDown[hand] && key_dest == key_game;
}

} // namespace qvr::input

// Thumbstick locomotion. Running or walking as with the keyboard (walking: cl_alwaysrun 0, the
// default; the speed button switches): running is cl_movespeedkey times the speed, and the stick, being
// analog, moves at cl_forwardspeed in every direction (as the old engine did), on the response curve and never past
// sv_maxspeed (moveSpeed: a full push is the most the server gives; vr_move_autorun eases a walk into a run). The server steers by
// the head (.v_viewangle): with
// vr_movement_mode 1 the stick moves relative to the head; with 0 it moves where the moving hand
// (hands::moveHand) points, with 2 the left hand, 3 the right hand, expressed relative to the head. Either way, pointing what
// steers (the head in mode 1, else that hand) up or down while pushing forward swims up or down (from the old engine's VR_Move).
extern "C" void VR_AdjustMove(float* forwardmove, float* sidemove, float* upmove)
{
    const float m = glm::length(moveAxes);
    const bool running = ((in_speed.state & 1) != 0) != (cl_alwaysrun.value != 0.f);
    const float threshold = CLAMP(0.1f, vr_move_autorun_threshold.value, 1.f);
    const float dt = CLAMP(0.f, static_cast<float>(host_frametime), 0.1f);
    autorunBlend = !autorunOn(running) || m == 0.f ? 0.f
                   : m >= threshold                ? za::min(1.f, autorunBlend + dt / autorunRiseSeconds)
                                                   : za::max(0.f, autorunBlend - dt / autorunFallSeconds);
    if(m == 0.f)
    {
        lastStickSpeed = 0.f;
        return;
    }

    // The push's direction (length 1) at the curve's speed (moveSpeed); swimming up or down as fast relatively.
    const bool legacy = vr_move_legacy.value != 0.f;
    const float speed = legacy ? m * cl_forwardspeed.value * (running ? cl_movespeedkey.value : 1.f)
                               : moveSpeed(m, running, autorunBlend, vr_move_curve.value, moveMinSpeed());
    const float speedScale = cl_forwardspeed.value > 0.f ? speed / (m * cl_forwardspeed.value) : 0.f;
    const glm::vec2 dir = moveAxes / m;
    const hands::State& s = hands::current();

    float fwd = dir.y;
    float side = dir.x;

    const int mode = static_cast<int>(vr_movement_mode.value);
    // 2 the left hand, 3 the right hand; 0 the moving stick's (1: the head steers).
    const int hand = mode == 2 ? HAND_OFF : mode == 3 ? HAND_MAIN : hands::moveHand();
    if(s.valid && mode != 1)
    {
        glm::vec3 lfwd, lright, lup;
        hands::angleVectors(s.rot[hand], lfwd, lright, lup);

        // Pointing (nearly) straight up or down: steer with the hand's up vector instead.
        if(za::fabs(lfwd.z) > 0.8f)
        {
            if(lfwd.z < -0.8f)
            {
                lfwd = -lfwd;
            }
            else
            {
                lup = -lup;
            }
            za::genericSwap(lup, lfwd);
        }

        // Tilting the hand must not change the speed.
        const float fac = 1.f / za::fmax(za::fabs(lup.z), 0.2f);
        const glm::vec3 move = (dir.y * lfwd + dir.x * lright) * fac;

        glm::vec3 vfwd, vright, vup;
        hands::angleVectors({0.f, s.headAngles.y, 0.f}, vfwd, vright, vup);
        fwd = glm::dot(move, vfwd);
        side = glm::dot(move, vright);
        // (A rolled hand's compensation never past the push: the top speed stays the top.)
        if(const float l = za::sqrt(fwd * fwd + side * side); l > 1.f && !legacy)
        {
            fwd /= l;
            side /= l;
        }
    }

    *forwardmove += fwd * speed;
    *sidemove += side * speed;
    lastStickSpeed = speed * za::sqrt(fwd * fwd + side * side);

    if(s.valid)
    {
        // Swimming up or down follows what steers: the head (mode 1) or the steering hand.
        const glm::vec3 steerFwd = mode == 1 ? hands::forward(s.headAngles) : hands::forward(s.rot[hand]);
        *upmove += cl_upspeed.value * moveAxes.y * steerFwd.z * speedScale;
    }
}

namespace qvr::input
{

namespace
{
bool jumping = false; // roomscaleJump: the jump key held (the head rising)
} // namespace

void roomscaleJump(const hands::State& s)
{
    // (Seated: never; sitting up quickly would raise the head over the standing height: vr_seated.hpp.)
    const bool rising = vr_roomscale_jump.value && !seated::active() && vrActive() && s.valid && key_dest == key_game && !posing::active() && !holstersetup::active() &&
                        s.headVel.z > vr_roomscale_jump_threshold.value && s.headHeight > vr_height_calibration.value;
    if(rising != jumping)
    {
        jumping = rising;
        Cbuf_AddText(rising ? "+jump\n" : "-jump\n");
    }
}

void parseHaptic()
{
    const int hand = MSG_ReadByte();
    const float delay = MSG_ReadFloat();
    const float duration = MSG_ReadFloat();
    const float frequency = MSG_ReadFloat();
    const float amplitude = MSG_ReadFloat();

    // At most 10 s ahead (a NaN or a huge delay would never come due: kept for ever), at most 64 waiting.
    if((hand == HAND_OFF || hand == HAND_MAIN) && pendingHaptics.size() < 64)
    {
        const double wait = ZA_ISFINITE(delay) ? CLAMP(0.0, static_cast<double>(delay), 10.0) : 0.0;
        pendingHaptics.pushBack({realtime + wait, hand, duration, frequency, amplitude});
    }
}

} // namespace qvr::input
