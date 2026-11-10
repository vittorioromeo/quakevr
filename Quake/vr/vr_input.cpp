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

#include "vr_animrec.hpp"
#include "vr_testkit.hpp"
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_chainsaw.hpp"
#include "vr_magholster.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_gunsfx.hpp"
#include "vr_input.hpp"
#include "vr_doom.hpp"
#include "vr_main.hpp"
#include "vr_menuui.hpp"
#include "vr_motion.hpp"
#include "vr_holstersetup.hpp"
#include "vr_barrelmask.hpp"
#include "vr_bladeedit.hpp"
#include "vr_poselive.hpp"
#include "vr_posestudio.hpp"
#include "vr_posing.hpp"
#include "vr_progs.hpp"
#include "vr_teleport.hpp"
#include "vr_voicenotes.hpp"
#include "vr_seated.hpp"
#include "vr_timescale.hpp"
#include "vr_flashlight.hpp"
#include "vr_holsterzones.hpp"
#include "vr_toolgun.hpp"
#include "vr_arenacards.hpp"
#include "vr_waveeditor.hpp"
#include "vr_weaponstudio.hpp" // (its window pausing his own walking: VR_AdjustMove below)
#include "vr_flycam.hpp"       // (the flatscreen fly camera has his move keys too: VR_AdjustMove below)
#include "vr_geom.hpp"

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
#include "vr_api.h"


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

// Smooth + Teleport (vr_teleport_enabled 2; vr_teleport.cpp; ROUND24.md "Teleport and smooth locomotion together"): the
// turning stick pushed forward aims a teleport with its hand, back to the middle lands, pulled back cancels. Its left and
// right still turn, never while it aims nor until it is back in the middle after a cancel; the moving stick doesn't walk
// while it aims; a push that began as a turn never becomes an aim. Swimming, its forward swims up as ever (+moveup).
struct TeleportStick
{
    bool armed{false};    // back in the middle since the last turn or aim: a push forward may aim
    bool aiming{false};
    bool settling{false}; // cancelled: the stick is the teleport's until back in the middle
    bool ran{false};      // routeSticks's walking branch ran this frame
};
TeleportStick teleStick;

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

// vr_move_snap: a stick's direction within that many degrees of an axis (straight ahead, back, a side) exactly on it, the
// rest of the 45 degrees to the diagonal spread over what is left: no jump, the diagonals stay, the length is kept.
[[nodiscard]] glm::vec2 moveSnap(glm::vec2 stick)
{
    const float snap = CLAMP(0.f, vr_move_snap.value, 40.f);
    if(snap <= 0.f)
    {
        return stick;
    }
    const float deg = glm::degrees(za::atan2(stick.x, stick.y));
    const float axis = 90.f * glm::floor(deg / 90.f + 0.5f);
    const float off = deg - axis;
    const float away = za::fabs(off);
    const float kept = away <= snap ? 0.f : za::copysign((away - snap) * 45.f / (45.f - snap), off);
    if(kept == 0.f)
    {
        // (Exactly: a straight push's side part is 0, not a float's cosine of 90 degrees.)
        const float len = glm::length(stick);
        const int quarter = static_cast<int>(axis / 90.f);
        return quarter == 0 ? glm::vec2{0.f, len}
               : quarter == 1 ? glm::vec2{len, 0.f}
               : quarter == -1 ? glm::vec2{-len, 0.f}
                               : glm::vec2{0.f, -len};
    }
    const float rad = glm::radians(axis + kept);
    return glm::length(stick) * glm::vec2{glm::sin(rad), glm::cos(rad)};
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
    return moveSnap(stick) * (m / len);
}

// vr_move_hold: a full push kept as the thumb slips off it (his thumb held hard forward, 2026-11-02: 0.55..0.75 for a
// tenth of a second about three times a second, each a run up to 28% slower for a moment). It stays full for vr_move_hold
// seconds, then falls to the push at moveHoldFall a second; under moveHoldLeast (letting go, creeping) it follows at
// once. A push that never was full follows at once too: slow walking answers as ever.
constexpr float moveHoldFall = 4.f;
constexpr float moveHoldLeast = 0.25f;
constexpr float moveHoldFull = 0.999f;
struct MoveHold
{
    float push{0.f};
    float left{0.f};
};
MoveHold moveHeld;

[[nodiscard]] glm::vec2 moveHold(glm::vec2 axes, float dt)
{
    MoveHold& h = moveHeld;
    const float m = glm::length(axes);
    const float hold = vr_move_hold.value;
    if(hold <= 0.f || m < moveHoldLeast)
    {
        h = MoveHold{m, 0.f};
        return axes;
    }
    if(m >= moveHoldFull)
    {
        h = MoveHold{1.f, hold};
        return axes;
    }
    if(h.push <= m)
    {
        h.push = m;
        return axes;
    }
    if(h.left > 0.f)
    {
        h.left -= dt;
    }
    else
    {
        h.push = za::max(m, h.push - moveHoldFall * dt);
    }
    return axes * (h.push / m);
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
            if(!gunsfx::punchOwns(it->hand)) // (a shot's own punch over its flat buzz: vr_gunsfx.cpp)
            {
                haptics::pulse(it->hand, it->seconds, it->amplitude, it->frequency);
            }
            it = pendingHaptics.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// vr_move_report [label]: Move Towards' setting, the yaws (world degrees) of what can steer, and where the player is walking.
void moveReport_f()
{
    const hands::State& s = hands::current();
    Con_Printf("move report %s: mode %d head %.1f torso %.1f left %.1f right %.1f walk %.1f speed %.0f\n", Cmd_Argv(1),
               static_cast<int>(vr_movement_mode.value), s.headAngles.y, s.bodyYaw, s.rot[HAND_OFF].y, s.rot[HAND_MAIN].y,
               glm::degrees(za::atan2(cl.velocity[1], cl.velocity[0])), za::sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]));
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
// curved, and the checks (monotonic, nothing inside the deadzone, full at 80%, about half at 55% with 1.6, running's
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
    int mode{1};  // vr_movement_mode (1 the head steers; 0, 2, 3 a hand; 4 the torso)
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
constexpr float stickTestOuter = 20.f;   // vr_deadzone_outer's default (a full push from 80%)
constexpr float stickTestFull = 0.9f - stickTestOuter / 100.f; // the travel 0..1 spans, past the inner 10%

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
    float curve{0.f}, autorun{0.f}, threshold{0.f}, inner{0.f}, outer{0.f}, least{0.f}, snap{0.f}, hold{0.f};
    // vr_stick_run_test: its run (-1: the curve test), that run's frames, full speed reached (at frame), the most and
    // the least speed since, the least asked
    int run{-1};
    int runFrames{0};
    int runReachedAt{-1};
    int runLowAt{0};
    float runTop{0.f}, runLow{0.f}, runAskedLow{0.f};
    // The drawn steps (run 0): the client's origin last frame, the frames left unmeasured after a put-back, and the steps'
    // count, sum, sum of squares, least and most.
    glm::vec3 drawnLast{0.f};
    int drawnSkip{0};
    int drawnSteps{0};
    double drawnSum{0.0}, drawnSquares{0.0};
    float drawnLeast{0.f}, drawnMost{0.f};
    // The sweep's case and its worst heading and speed errors; the line's distance to go, where it began and its height.
    int sweepCase{0};
    float sweepHeading{0.f}, sweepSpeed{0.f}, sweepPushed{0.f};
    float lineLength{0.f}, lineZLow{0.f}, lineZHigh{0.f}, lineZStep{0.f};
    int lineAir{0};
    int lineStrays{0};
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

// What a case should ask on the ground, worked out apart from moveSpeed: the push past 10% to 80% onto 0..1, on the
// curve, from the least speed (44: 1.1 sv_stopspeed sv_friction / sv_accelerate) to 200 walking or 320 running;
// walking into running (settled: run past the 0.9 threshold, else the walk over 0..0.9).
[[nodiscard]] float stickExpected(const StickCase& c)
{
    const float m = CLAMP(0.f, (c.push - 0.1f) / stickTestFull, 1.f);
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
    Cvar_SetValueQuick(&vr_move_snap, t.snap);
    Cvar_SetValueQuick(&vr_move_hold, t.hold);
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
    qvr::testkit::verdict(stickTest.run == 100 ? "stick line test" : stickTest.run >= 0 ? "stick run test" : "stick curve test",
                          stickTest.passed, stickTest.failed);
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
                full = full && (r < 0.805f || za::fabs(v - top) < 0.01f);
            }
            q_snprintf(what, sizeof(what), "curve %.1f %s: monotonic", p, run ? "run" : "walk");
            stickCheck(monotonic, what);
            q_snprintf(what, sizeof(what), "curve %.1f %s: nothing inside the deadzone", p, run ? "run" : "walk");
            stickCheck(dead, what);
            q_snprintf(what, sizeof(what), "curve %.1f %s: full speed (%.0f) from an 80%% push", p, run ? "run" : "walk", top);
            stickCheck(full, what);
        }
    }
    const float half = moveSpeed(glm::length(moveStick({0.f, 0.55f})), false, 0.f, 1.6f) / stickTestWalk;
    Con_Printf("stick curve: curve 1.6 at a 55%% push: %.3f of the top speed\n", half);
    stickCheck(half > 0.45f && half < 0.55f, "curve 1.6: about half speed at a 55% push");

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

// The settings both stick tests measure against (saved, put back by stickTestRestore).
void stickTestSetUp()
{
    StickTest& t = stickTest;
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
    t.snap = vr_move_snap.value;
    t.hold = vr_move_hold.value;
    // (The curve test holds each push; the run test sets its own.)
    Cvar_SetValueQuick(&vr_move_hold, 0.f);
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
    Cvar_SetValueQuick(&vr_deadzone_outer, stickTestOuter);
    Cvar_SetValueQuick(&vr_move_min_speed, -1.f);
}

void stickTest_f()
{
    StickTest& t = stickTest;
    if(t.active)
    {
        Con_Printf("stick curve: already running\n");
        return;
    }
    stickTestSetUp();

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

// ---- vr_stick_run_test [x y z] [outer [snap [hold]]]: a run held at a full push (his voice notes of 2026-11-01: "once
// every few steps, the player slows down a little bit ... like a hitch"; of 2026-11-02: hitches still, and "a bit
// angled even if my thumbstick is completely straight"; ROUND23.md "Running hitch" and "Thumbstick walking"). Three runs
// of 10 s (720 frames) forward, running (320), the head steering, from x y z (default: where the player stands; flat floor
// round it for 96 units): 0 the stick pushed fully forward, still; 1 a thumb held hard forward as it slips: the push's
// length 0.82..1.0, its angle +-8 degrees; 2 his thumb as recorded (stickRunThumb, 0.61..0.91). Each: once the speed is
// full (320 within 1%), it never dips more than 2% under the most it reached. Each time the player is 96 units from the
// start it is put back there, its velocity kept (a run of 3200 units in a room). In run 0 the steps the client draws must
// be even: within 3% (rms) of their mean (at vr_fixed_frames_rate 120 and vr_fixed_frames_jitter 0.2, a headset's, what
// vr_display_clock takes away). Then the sweep: stickSweepCases pushes from rest, each angle at two lengths, the head level
// and 40 degrees down: the velocity's heading within 0.5 degrees of what the stick's angle asks through a straight zone
// of 15 degrees, the speed within 1.5%. outer, snap, hold: vr_deadzone_outer, vr_move_snap, vr_move_hold (the responses
// before these fixes: 5, 0, 0), else their defaults. The settings the curve test sets (stickTestSetUp), put back after.
// Ends "stick run test: N passed, M failed".
constexpr int stickRunFrames = 720;      // a run: 10 s
constexpr int stickRunReachFrames = 216; // not full by then: FAIL (a safety net)
constexpr float stickRunDip = 0.02f;     // the most a run may dip under its top
constexpr float stickRunRoom = 96.f;     // how far from the start the player runs before it is put back
constexpr int stickRunCount = 3;
constexpr float stickDrawnEven = 0.03f;  // the most the drawn steps may spread (rms over their mean)
constexpr int stickDrawnSkip = 6;        // frames not measured from a put-back (the client catching up with it)

// His thumb in his run of 2026-11-02 (the note's replay, frames 1301.., every 0.1 s, the push's length).
constexpr float stickRunThumb[] = {0.74f, 0.87f, 0.65f, 0.77f, 0.87f, 0.61f, 0.80f, 0.91f, 0.86f, 0.67f, 0.79f, 0.85f, 0.70f};
constexpr int stickRunThumbCount = static_cast<int>(sizeof(stickRunThumb) / sizeof(stickRunThumb[0]));

// The push on frame f of run r: 0 full, still; 1 a slow wobble and a tremor (0.86..1.0) and, every 40 frames (0.55 s)
// for 7 (0.1 s), a slip to 0.82; the angle swaying +-8 degrees; 2 his thumb's lengths, the same sway.
[[nodiscard]] glm::vec2 stickRunPush(int r, int f)
{
    if(r == 0)
    {
        return {0.f, 1.f};
    }
    constexpr float tau = 6.2831853f;
    const float s = static_cast<float>(f) / 72.f;
    float len = f % 40 < 7 ? 0.82f : 0.93f + 0.05f * glm::sin(tau * 1.7f * s) + 0.02f * glm::sin(tau * 7.3f * s);
    if(r == 2)
    {
        const float at = s * 10.f;
        const int i = static_cast<int>(at);
        const float u = at - static_cast<float>(i);
        const float a = stickRunThumb[i % stickRunThumbCount];
        len = a + (stickRunThumb[(i + 1) % stickRunThumbCount] - a) * u;
    }
    const float a = 8.f * (tau / 360.f) * glm::sin(tau * 0.9f * s);
    return {len * glm::sin(a), len * glm::cos(a)};
}

void stickRunPlace(edict_t* p, bool still);

// The sweep: the angles (degrees clockwise from straight ahead), the lengths and the head's pitches.
constexpr float stickSweepAngles[] = {0.f, 5.f, 10.f, 14.f, -5.f, -10.f, -14.f, 20.f, 30.f, 45.f, 60.f, 80.f, 90.f, -90.f, 135.f, 180.f};
constexpr float stickSweepLengths[] = {1.f, 0.6f};
constexpr float stickSweepPitches[] = {0.f, 40.f};
constexpr int stickSweepAngleCount = static_cast<int>(sizeof(stickSweepAngles) / sizeof(stickSweepAngles[0]));
constexpr int stickSweepCases = stickSweepAngleCount * 2 * 2;
constexpr int stickSweepRest = 4;     // frames at rest with the head turned, before a push
constexpr float stickSweepPushSeconds = 14.f / 72.f; // pushed for: the speed full (Quake's acceleration) in under 60 units
constexpr float stickSweepZone = 15.f; // vr_move_snap's default, worked out apart from moveSnap
constexpr float stickSweepHeadingMost = 0.5f;
constexpr float stickSweepSpeedMost = 0.015f;

// The heading a stick's angle asks through the straight zone, off the head's (clockwise, degrees).
[[nodiscard]] float stickSweepWanted(float angle)
{
    const float axis = 90.f * glm::floor(angle / 90.f + 0.5f);
    const float off = angle - axis;
    const float away = za::fabs(off);
    return axis + (away <= stickSweepZone ? 0.f : za::copysign((away - stickSweepZone) * 45.f / (45.f - stickSweepZone), off));
}

// Each frame of the sweep (the head's pitch by vr_mock_look: a real head's stays its own; level or not, it must not matter).
void stickSweepTick(edict_t* p)
{
    StickTest& t = stickTest;
    const int perLength = stickSweepAngleCount;
    const float angle = stickSweepAngles[t.sweepCase % perLength];
    const float length = stickSweepLengths[(t.sweepCase / perLength) % 2];
    const float pitch = stickSweepPitches[t.sweepCase / (perLength * 2)];
    if(t.runFrames == 0)
    {
        t.stick = glm::vec2{0.f};
        Cbuf_AddText(va("vr_mock_look %g 0\n", pitch));
    }
    ++t.runFrames;
    if(t.runFrames <= stickSweepRest)
    {
        stickRunPlace(p, true); // (at rest: the last case's push is gone a frame late)
        t.sweepPushed = 0.f;
    }
    if(t.runFrames == stickSweepRest)
    {
        const float rad = glm::radians(angle);
        t.stick = length * glm::vec2{glm::sin(rad), glm::cos(rad)};
        return;
    }
    t.sweepPushed += static_cast<float>(host_frametime);
    if(t.runFrames < stickSweepRest || t.sweepPushed < stickSweepPushSeconds)
    {
        return;
    }
    // The heading against the head's yaw now; the speed against what the push asks (running, from the least speed).
    const float speed = stickPlayerSpeed();
    const float heading = geom::yawDelta(hands::current().headAngles.y, glm::degrees(za::atan2(p->v.velocity[1], p->v.velocity[0])));
    const float err = za::fabs(geom::yawDelta(heading, stickSweepWanted(angle)));
    const float least = 1.1f * sv_stopspeed.value * sv_friction.value / sv_accelerate.value;
    const float m = CLAMP(0.f, (length - 0.1f) / stickTestFull, 1.f);
    const float wantedSpeed = least + (stickTestRun - least) * za::pow(m, vr_move_curve.value);
    const float speedErr = za::fabs(speed / wantedSpeed - 1.f);
    t.sweepHeading = za::max(t.sweepHeading, err);
    t.sweepSpeed = za::max(t.sweepSpeed, speedErr);
    if(pitch == 0.f && length == 1.f)
    {
        Con_Printf("stick run: sweep: stick %+.0f deg: heading %+.2f off the head's (asks %+.2f), speed %.1f\n", angle, heading,
                   stickSweepWanted(angle), speed);
    }
    char what[200];
    q_snprintf(what, sizeof(what), "sweep: stick %+.0f deg, push %.1f, head pitch %.0f: heading %.2f deg off (%.2f the most), speed %.1f (asks %.1f)",
               angle, length, pitch, err, stickSweepHeadingMost, speed, wantedSpeed);
    stickCheck(err <= stickSweepHeadingMost && speedErr <= stickSweepSpeedMost, what);
    t.runFrames = 0;
    if(++t.sweepCase < stickSweepCases)
    {
        return;
    }
    Con_Printf("stick run: sweep: %d pushes: worst heading error %.2f deg, worst speed error %.2f%%\n", stickSweepCases,
               t.sweepHeading, 100.f * t.sweepSpeed);
    Cbuf_AddText("vr_mock_look 0 0\n");
    stickTestFinish();
}

// vr_stick_line_test x y z <length>: from x y z the stick fully forward along where the head looks (vr_mock_turn_to first),
// running, without a put-back: over a real floor and its seams (his arena's). Once full, the speed within 2% of its
// top, on the ground all the way, the height never moving more than 0.1 unit a frame, until <length> units or blocked (a
// wall: the line ends there, what it crossed counts). Ends "stick line test: N passed, M failed".
constexpr int stickLineRun = 100; // StickTest::run's value
constexpr float stickLineZStep = 0.1f;
constexpr int stickLineStrayLines = 14;

void stickLine_f()
{
    if(stickTest.active || Cmd_Argc() != 5)
    {
        Con_Printf(stickTest.active ? "stick line: a stick test is already running\n" : "usage: vr_stick_line_test x y z <length>\n");
        return;
    }
    edict_t* p = stickPlayer();
    if(!p || !vrActive())
    {
        Con_Printf("stick line: no player in VR (a map, in VR)\n");
        return;
    }
    stickTestSetUp();
    StickTest& t = stickTest;
    Cvar_SetValueQuick(&cl_alwaysrun, 1.f);
    Cvar_SetQuick(&vr_deadzone_outer, vr_deadzone_outer.default_string);
    t.start = {static_cast<float>(atof(Cmd_Argv(1))), static_cast<float>(atof(Cmd_Argv(2))), static_cast<float>(atof(Cmd_Argv(3)))};
    t.lineLength = static_cast<float>(atof(Cmd_Argv(4)));
    t.active = true;
    t.run = stickLineRun;
    t.runReachedAt = -1;
    t.runLow = 1e9f;
    t.lineZLow = 1e9f;
    t.lineZHigh = -1e9f;
    t.stick = glm::vec2{0.f, 1.f};
    stickRunPlace(p, true);
}

void stickLineTick(edict_t* p)
{
    StickTest& t = stickTest;
    const float speed = stickPlayerSpeed();
    const float z = p->v.origin[2];
    ++t.runFrames;
    if(t.runReachedAt < 0)
    {
        if(speed >= stickTestRun * 0.99f - 1.f)
        {
            t.runReachedAt = t.runFrames;
            t.lineZHigh = t.lineZLow = z;
        }
        else if(t.runFrames >= stickRunReachFrames)
        {
            Con_Printf("stick line: FAIL never full (%.1f after %d frames)\n", speed, t.runFrames);
            ++t.failed;
            stickTestFinish();
        }
        t.drawnLast = glm::vec3{p->v.origin[0], p->v.origin[1], z};
        return;
    }
    const float dx = p->v.origin[0] - t.start.x;
    const float dy = p->v.origin[1] - t.start.y;
    const float gone = za::sqrt(dx * dx + dy * dy);
    const bool blocked = speed < stickTestRun * 0.5f; // (a wall: stopped; a seam's catch would show in the dip first)
    if(!blocked)
    {
        t.runTop = za::max(t.runTop, speed);
        if(speed < t.runLow)
        {
            t.runLow = speed;
            t.runLowAt = t.runFrames;
        }
        t.lineZLow = za::min(t.lineZLow, z);
        t.lineZHigh = za::max(t.lineZHigh, z);
        t.lineZStep = za::max(t.lineZStep, za::fabs(z - t.drawnLast.z));
        t.lineAir += (static_cast<int>(p->v.flags) & FL_ONGROUND) ? 0 : 1;
        // (Where it strays from the speed asked: the first few frames' places, to look at.)
        if(za::fabs(speed / stickTestRun - 1.f) > stickRunDip * 0.5f && t.lineStrays++ < stickLineStrayLines)
        {
            Con_Printf("stick line: frame %d at %.1f %.1f %.2f (%.0f units): speed %.1f, velocity %.1f %.1f %.1f, %s\n", t.runFrames,
                       p->v.origin[0], p->v.origin[1], z, gone, speed, p->v.velocity[0], p->v.velocity[1], p->v.velocity[2],
                       (static_cast<int>(p->v.flags) & FL_ONGROUND) ? "on the ground" : "in the air");
        }
    }
    t.drawnLast = glm::vec3{p->v.origin[0], p->v.origin[1], z};
    if(!blocked && gone < t.lineLength)
    {
        return;
    }
    const float dip = t.runTop > 0.f ? 1.f - t.runLow / t.runTop : 1.f;
    Con_Printf("stick line: %.0f units%s to %.0f %.0f: speed %.1f..%.1f (dip %.2f%%), height %.2f..%.2f, most a frame %.3f, "
               "frames in the air %d\n",
               gone, blocked ? " (blocked)" : "", p->v.origin[0], p->v.origin[1], t.runLow, t.runTop, 100.f * dip, t.lineZLow,
               t.lineZHigh, t.lineZStep, t.lineAir);
    char what[120];
    q_snprintf(what, sizeof(what), "line: dipped %.2f%%, %.3f units a frame in height, %d frames in the air", 100.f * dip,
               t.lineZStep, t.lineAir);
    stickCheck(dip <= stickRunDip && t.lineZStep <= stickLineZStep && t.lineAir == 0, what);
    stickTestFinish();
}

void stickRunPlace(edict_t* p, bool still)
{
    p->v.origin[0] = stickTest.start.x;
    p->v.origin[1] = stickTest.start.y;
    if(still)
    {
        p->v.origin[2] = stickTest.start.z;
        VectorCopy(vec3_origin, p->v.velocity);
    }
    progs::ServerVm serverVm; // (the input's frame: the client's VM, or none, is in)
    SV_LinkEdict(p, false);
}

void stickRunStart(int r)
{
    StickTest& t = stickTest;
    t.run = r;
    t.runFrames = 0;
    t.runReachedAt = -1;
    t.runTop = 0.f;
    t.runLow = 1e9f;
    t.runAskedLow = 1e9f;
    t.drawnSteps = 0;
    t.drawnSum = t.drawnSquares = 0.0;
    t.drawnLeast = 1e9f;
    t.drawnMost = 0.f;
    t.drawnSkip = stickDrawnSkip;
    t.stick = stickRunPush(r, 0);
    if(edict_t* p = stickPlayer())
    {
        stickRunPlace(p, true);
    }
}

void stickRun_f()
{
    if(stickTest.active)
    {
        Con_Printf("stick run: a stick test is already running\n");
        return;
    }
    edict_t* p = stickPlayer();
    if(!p || !vrActive())
    {
        Con_Printf("stick run: no player in VR (a map, in VR)\n");
        return;
    }
    stickTestSetUp();
    StickTest& t = stickTest;
    Cvar_SetValueQuick(&cl_alwaysrun, 1.f);
    const int argc = Cmd_Argc();
    // (x y z first, or none: outer, snap and hold after them.)
    const int settings = argc >= 4 ? 4 : 1;
    Cvar_SetQuick(&vr_deadzone_outer, argc > settings ? Cmd_Argv(settings) : vr_deadzone_outer.default_string);
    Cvar_SetQuick(&vr_move_snap, argc > settings + 1 ? Cmd_Argv(settings + 1) : vr_move_snap.default_string);
    Cvar_SetQuick(&vr_move_hold, argc > settings + 2 ? Cmd_Argv(settings + 2) : vr_move_hold.default_string);
    t.start = argc >= 4 ? glm::vec3{static_cast<float>(atof(Cmd_Argv(1))), static_cast<float>(atof(Cmd_Argv(2))),
                                    static_cast<float>(atof(Cmd_Argv(3)))}
                        : glm::vec3{p->v.origin[0], p->v.origin[1], p->v.origin[2]};
    if(p->v.movetype == static_cast<float>(MOVETYPE_NOCLIP))
    {
        p->v.movetype = static_cast<float>(MOVETYPE_WALK);
    }
    Con_Printf("stick run: vr_deadzone_outer %g, vr_move_snap %g, vr_move_hold %g, vr_display_clock %g, from %.0f %.0f %.0f\n",
               vr_deadzone_outer.value, vr_move_snap.value, vr_move_hold.value, vr_display_clock.value, t.start.x, t.start.y,
               t.start.z);
    t.active = true;
    stickRunStart(0);
}

// Each frame of a run: the push, the room, then full speed reached and the least and most since; after 720 frames its
// line and checks, then the next run.
void stickRunTick()
{
    StickTest& t = stickTest;
    edict_t* p = stickPlayer();
    if(t.run == stickLineRun)
    {
        stickLineTick(p);
        return;
    }
    if(t.run == stickRunCount)
    {
        stickSweepTick(p);
        return;
    }
    const float speed = stickPlayerSpeed();
    ++t.runFrames;
    t.stick = stickRunPush(t.run, t.runFrames);
    const float dx = p->v.origin[0] - t.start.x;
    const float dy = p->v.origin[1] - t.start.y;
    if(dx * dx + dy * dy > stickRunRoom * stickRunRoom)
    {
        stickRunPlace(p, false);
        t.drawnSkip = stickDrawnSkip;
    }
    // The step the client draws this frame (the camera's, against the world).
    const glm::vec3 drawn{cl_entities[cl.viewentity].origin[0], cl_entities[cl.viewentity].origin[1], cl_entities[cl.viewentity].origin[2]};
    if(t.drawnSkip > 0)
    {
        --t.drawnSkip;
    }
    else if(t.runReachedAt >= 0)
    {
        const float step = glm::length(drawn - t.drawnLast);
        ++t.drawnSteps;
        t.drawnSum += step;
        t.drawnSquares += static_cast<double>(step) * step;
        t.drawnLeast = za::min(t.drawnLeast, step);
        t.drawnMost = za::max(t.drawnMost, step);
    }
    t.drawnLast = drawn;
    static constexpr const char* names[stickRunCount] = {"full push, still", "a thumb held hard forward, slipping", "his thumb as recorded"};
    char what[200];
    if(t.runReachedAt < 0)
    {
        if(speed >= stickTestRun * 0.99f - 1.f)
        {
            t.runReachedAt = t.runFrames;
        }
        else if(t.runFrames >= stickRunReachFrames)
        {
            q_snprintf(what, sizeof(what), "run %d (%s): never full (%.1f after %d frames, asked %.1f)", t.run,
                       names[t.run], speed, t.runFrames, lastStickSpeed);
            stickCheck(false, what);
            t.runFrames = stickRunFrames; // (to the next run)
        }
    }
    if(t.runReachedAt >= 0)
    {
        t.runTop = za::max(t.runTop, speed);
        if(speed < t.runLow)
        {
            t.runLow = speed;
            t.runLowAt = t.runFrames;
        }
        t.runAskedLow = za::min(t.runAskedLow, lastStickSpeed);
    }
    if(t.runFrames < stickRunFrames)
    {
        return;
    }
    if(t.runReachedAt >= 0)
    {
        const float dip = t.runTop > 0.f ? 1.f - t.runLow / t.runTop : 1.f;
        Con_Printf("stick run: run %d (%s): full at frame %d; speed %.1f..%.1f (dip %.2f%%, the least at frame %d), "
                   "asked at least %.1f\n",
                   t.run, names[t.run], t.runReachedAt, t.runLow, t.runTop, 100.f * dip, t.runLowAt, t.runAskedLow);
        q_snprintf(what, sizeof(what), "run %d (%s): dipped %.2f%% (%.1f under %.1f), at most %.0f%%", t.run,
                   names[t.run], 100.f * dip, t.runLow, t.runTop, 100.f * stickRunDip);
        stickCheck(dip <= stickRunDip, what);
        if(t.run == 0 && t.drawnSteps > 0)
        {
            const double mean = t.drawnSum / t.drawnSteps;
            const double spread = za::sqrt(za::max(0.0, t.drawnSquares / t.drawnSteps - mean * mean)) / za::max(mean, 1e-6);
            Con_Printf("stick run: run 0: drawn steps %d, mean %.3f units, %.3f..%.3f, rms %.2f%% of the mean\n", t.drawnSteps,
                       mean, t.drawnLeast, t.drawnMost, 100.0 * spread);
            q_snprintf(what, sizeof(what), "run 0: the drawn steps spread %.2f%%, at most %.0f%%", 100.0 * spread, 100.f * stickDrawnEven);
            stickCheck(spread <= stickDrawnEven, what);
        }
    }
    stickRunStart(t.run + 1); // (run stickRunCount: the sweep)
    if(t.run == stickRunCount)
    {
        t.runFrames = 0;
        t.sweepCase = 0;
    }
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
    if(t.run >= 0)
    {
        stickRunTick();
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
    const edict_t* p = stickPlayer();
    const float speed = stickPlayerSpeed();
    const float heading = p && speed > 1.f
                              ? geom::yawDelta(hands::current().headAngles.y,
                                               glm::degrees(za::atan2(p->v.velocity[1], p->v.velocity[0])))
                              : 0.f;
    Con_Printf("stick: push %.2f at %.1f deg -> %.3f, asked %.0f (top %.0f walk, %.0f run), moved %.0f, heading %.1f deg "
               "off the head's\n",
               glm::length(raw), glm::degrees(za::atan2(raw.x, raw.y)), glm::length(moveAxes), lastStickSpeed,
               moveTopSpeed(false), moveTopSpeed(true), speed, heading);
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
    Cmd_AddCommand("vr_stick_run_test", stickRun_f);
    Cmd_AddCommand("vr_stick_line_test", stickLine_f);
    Cmd_AddCommand("vr_move_report", moveReport_f);
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
    // And Paint Barrel Mask (vr_barrelmask.cpp).
    if(barrelmask::button(h, posingButton(b.button), now))
    {
        logButton(h, b, now, b.key[h], "Paint Barrel Mask");
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
    // The Arena Wave Editor: a trigger while it's open is its click (vr_waveeditor.cpp).
    if(waveeditor::button(h, posingButton(b.button), now))
    {
        logButton(h, b, now, b.key[h], "the wave editor");
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
    // With the animation recorder armed, its record button (vr_ar_button) starts and ends a recording instead.
    if(animrec::button(h, b.button, now))
    {
        logButton(h, b, now, b.key[h], "the animation recorder");
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

// The turning stick taken from aiming (a menu, a mode, swimming, another Teleport choice): an aim under way cancelled.
void teleportStickOff()
{
    if(teleStick.aiming)
    {
        teleport::stickAim(1 - hands::moveHand(), false, true);
    }
    teleStick = TeleportStick{};
}

// The turning stick `st` of hand `hand` in the game (TeleportStick): true while it is the teleport's (no turning, no
// +moveup, no walking).
[[nodiscard]] bool teleportStick(glm::vec2 st, int hand)
{
    TeleportStick& t = teleStick;
    t.ran = true;
    const edict_t* p = progs::localPlayer(); // (the aim's sweep needs this machine's server)
    if(!teleport::stickMode() || !p || p->v.waterlevel >= 2.f)
    {
        teleportStickOff();
        t.ran = true;
        return false;
    }
    const float len = glm::length(st);
    if(len < 0.3f)
    {
        t.armed = true;
        t.settling = false;
    }
    if(t.settling)
    {
        return true;
    }
    if(!t.aiming)
    {
        if(!t.armed || st.y <= 0.7f || za::fabs(st.x) >= 0.5f * st.y)
        {
            if(za::fabs(st.x) > 0.3f)
            {
                t.armed = false; // turning: no aim until back in the middle
            }
            return false;
        }
        t.aiming = true;
        t.armed = false;
        snapTurnArmed = false;
    }
    if(st.y < -0.5f)
    {
        t.aiming = false;
        t.settling = true;
        teleport::stickAim(hand, false, true);
        return true;
    }
    if(len < 0.3f)
    {
        t.aiming = false;
        teleport::stickAim(hand, false, false);
        return false;
    }
    teleport::stickAim(hand, true, false);
    return true;
}

void routeSticksTo(const InputState& in, const HandInput& off, const HandInput& main, bool active);

// The sticks: the menu's keys, a tool's or mode's, or walking and turning.
void routeSticks(const InputState& in, const HandInput& off, const HandInput& main, bool active)
{
    teleStick.ran = false;
    routeSticksTo(in, off, main, active);
    if(!teleStick.ran)
    {
        teleportStickOff();
    }
}

void routeSticksTo(const InputState& in, const HandInput& off, const HandInput& main, bool active)
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
    else if(waveeditor::shown())
    {
        // The Arena Wave Editor open: the sticks are its wheel (no walking, no turning).
        for(StickKey& k : stickKeys)
        {
            stickKey(k, 0.f, false);
        }
        moveAxes = glm::vec2{0.f};
        (void)waveeditor::sticks(in.hands[HAND_OFF].stick, in.hands[HAND_MAIN].stick);
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
    else if(barrelmask::active())
    {
        // Paint Barrel Mask: the player stands still; the pointing hand's stick sizes the brush.
        for(StickKey& k : stickKeys)
        {
            stickKey(k, 0.f, false);
        }
        moveAxes = glm::vec2{0.f};
        barrelmask::sticks(in.hands[HAND_OFF].stick, in.hands[HAND_MAIN].stick);
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
        // (Smooth + Teleport: the turning stick aiming is no +moveup or +movedown, no turn and no walk.)
        const bool aims = teleportStick(main.stick, 1 - hands::moveHand());
        stickKey(stickKeys[0], aims ? 0.f : main.stick.y, false);
        stickKey(stickKeys[1], aims ? 0.f : -main.stick.y, false);
        stickKey(stickKeys[2], 0.f, false);
        stickKey(stickKeys[3], 0.f, false);
        // (vr_stick_curve_test drives the moving stick while it runs.)
        const glm::vec2 moving = aims ? glm::vec2{0.f} : stickTest.active ? stickTest.stick : off.stick;
        moveAxes = vr_move_legacy.value != 0.f
                       ? glm::vec2{deadzone(moving.x), deadzone(moving.y)}
                       : moveHold(moveStick(moving), CLAMP(0.f, static_cast<float>(host_frametime), 0.1f));
        stickDebug(off.stick);
        if(active && !aims)
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
                !barrelmask::button(h, posing::Button::Menu, menuNow) &&
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
// (hands::moveHand) points, with 2 the left hand, 3 the right hand, with 4 where the torso faces (hands::State::bodyYaw),
// expressed relative to the head. Either way, pointing what steers up or down while pushing forward swims up or down (from
// the old engine's VR_Move): that hand, or the head in modes 1 and 4 (the torso's direction is only a yaw).
extern "C" void VR_AdjustMove(float* forwardmove, float* sidemove, float* upmove)
{
    // The Weapon Studio's window owns his move keys while it is open -- W A S D and Q E fly its drawn camera there
    // (vr_weaponstudio.cpp) -- so his own walking is paused for those frames, whatever his keys and his sticks were
    // pressed to. Nothing here is kept, so the window's Close gives the walking back exactly as it was; his looking is
    // untouched either way.
    // The flatscreen fly camera likewise: its W A S D fly the camera, and he stands still (vr_flycam.cpp).
    if(weaponstudio::active() || flycam::on())
    {
        *forwardmove = 0.f;
        *sidemove = 0.f;
        *upmove = 0.f;
        lastStickSpeed = 0.f;
        return;
    }

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
    // 2 the left hand, 3 the right hand; 0 the moving stick's (1: the head steers; 4: the torso).
    const bool handSteers = mode != 1 && mode != 4;
    const int hand = mode == 2 ? HAND_OFF : mode == 3 ? HAND_MAIN : hands::moveHand();
    if(s.valid && mode == 4)
    {
        glm::vec3 tfwd, tright, tup, vfwd, vright, vup;
        hands::angleVectors({0.f, s.bodyYaw, 0.f}, tfwd, tright, tup);
        hands::angleVectors({0.f, s.headAngles.y, 0.f}, vfwd, vright, vup);
        const glm::vec3 move = dir.y * tfwd + dir.x * tright;
        fwd = glm::dot(move, vfwd);
        side = glm::dot(move, vright);
    }
    else if(s.valid && handSteers)
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
        // Swimming up or down follows what steers: the steering hand, or the head (modes 1 and 4).
        const glm::vec3 steerFwd = handSteers ? hands::forward(s.rot[hand]) : hands::forward(s.headAngles);
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
