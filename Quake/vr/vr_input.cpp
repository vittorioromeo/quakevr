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

#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_chainsaw.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_input.hpp"
#include "vr_main.hpp"
#include "vr_menuui.hpp"
#include "vr_motion.hpp"
#include "vr_posing.hpp"
#include "vr_voicenotes.hpp"
#include "vr_timescale.hpp"
#include "vr_flashlight.hpp"

#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Copysign.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"


extern "C" qboolean keydown[MAX_KEYS]; // keys.c

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

void menuButton(int hand, bool now, bool before)
{
    if(!now || before)
    {
        return;
    }
    if(key_dest == key_menu && m_state != m_none && !M_WaitingForKeyBinding())
    {
        menuui::backToGame(hand);
    }
    else
    {
        escape();
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

InputState previous;
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
            hands::addTurn(x > 0.f ? -vr_snap_turn.value : vr_snap_turn.value);
            comfortMotion.snapPulse = 0.3f;
        }
    }
    else if(const float v = deadzone(x); v != 0.f)
    {
        comfortMotion.turn = za::min(1.f, za::fabs(v));
        // (In slow motion at its real-time speed with vr_timescale_turn_realtime or Sandevistan.)
        hands::addTurn(-v * static_cast<float>(host_frametime) * timescale::turnSpeedup() * 100.f * vr_turn_speed.value);
    }
}

// Once a frame, after the sticks: the comfort vignette's target (moving: the stick's push; turning: smooth turning's, or
// a snap's pulse, at once), eased in quickly and out more slowly.
void updateComfort()
{
    ComfortMotion& c = comfortMotion;
    const int mode = static_cast<int>(vr_comfort_vignette.value);
    const float dt = static_cast<float>(CLAMP(0.0, host_frametime, 0.1));
    const float moving = mode == 1 || mode == 2 ? za::min(1.f, glm::length(moveAxes)) : 0.f;
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
            if(Backend* be = backend(); be && !vr_disablehaptics.value)
            {
                be->haptic(it->hand, it->seconds, it->frequency, it->amplitude);
            }
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

} // namespace

namespace qvr::input
{

void init()
{
    Cmd_AddCommand("vr_checkbindings", checkBindings_f);
}

namespace
{
const InputState released; // (update: without VR, everything is released)
} // namespace

void update(const InputState& tracked)
{
    // Without VR, everything is released (keys held when the session ends must come up).
    const bool active = vrActive();
    const InputState& in = active ? tracked : released;

    // The sticks by what they do: the moving one (vr_stick_swap: the right), the turning one.
    const HandInput& off = in.hands[hands::moveHand()];
    const HandInput& main = in.hands[1 - hands::moveHand()];

    // Where the hands point at the menu, before the trigger clicks there.
    menuui::update(hands::current());

    for(int h = 0; h < HAND_COUNT; h++)
    {
        for(const ButtonKeys& b : buttonKeys)
        {
            const bool now = in.hands[h].*b.button;
            if(now != previous.hands[h].*b.button)
            {
                // The weapon posing mode takes the buttons pressed while it runs (vr_posing.cpp).
                if(posing::button(h, posingButton(b.button), now))
                {
                    logButton(h, b, now, b.key[h], "the weapon posing mode");
                    continue;
                }
                // The off hand's upper button at the mouth records a voice note instead (not while that hand holds
                // the flashlight or is at it on the head: its Y turns it round in the fist, clips it on the head or
                // takes it off; a note's release always ends it).
                if(h == HAND_OFF && b.button == &HandInput::secondary && (!now || !flashlight::wantsSecondary(HAND_OFF)) &&
                    voicenotes::offhandButton(now))
                {
                    logButton(h, b, now, b.key[h], "a voice note");
                    continue;
                }
                // With the motion recorder armed, its record button (vr_motion_button) starts and ends a take instead.
                if(motion::button(h, b.button, now))
                {
                    logButton(h, b, now, b.key[h], "the motion recorder");
                    continue;
                }
                // A stick press chosen to start bullet time (vr_bullettime_trigger) does only that in the game; its
                // bound key (LTHUMB: +speed, RTHUMB: +reloadmain by default) is never sent.
                if(b.button == &HandInput::stickClick && bullettime::stickPress(h, now))
                {
                    logButton(h, b, now, b.key[h], "bullet time");
                    continue;
                }
                // An empty hand at the cord's handle of the chainsaw in the other takes the cord (vr_chainsaw.cpp).
                if(b.button == &HandInput::grip && chainsaw::grip(h, now))
                {
                    continue;
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
                        continue;
                    }
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
        }

        if(in.hands[h].menu == previous.hands[h].menu || !posing::button(h, posing::Button::Menu, in.hands[h].menu))
        {
            menuButton(h, in.hands[h].menu, previous.hands[h].menu);
        }
    }

    // A grip pressed on the way to the flashlight takes it as the hand comes to rest there: the game saw the press,
    // it sees the grip let go now (the release is the flashlight's).
    flashlight::lateGrips();
    for(int g = 0; g < HAND_COUNT; g++)
    {
        if(flashlight::tookGrip(g))
        {
            keyEvent(buttonKeys[1].key[g], false);
        }
    }

    const bool menu = key_dest != key_game;
    if(menu)
    {
        // The off hand's stick navigates and changes values (the arrow keys). The main hand's only
        // scrolls (on a page with a scrollbar) or moves the selection up and down: its left and
        // right do nothing, so that navigating never changes a setting by accident (round 20).
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
        moveAxes = {deadzone(off.stick.x), deadzone(off.stick.y)};
        if(active)
        {
            turn(main.stick.x);
        }
    }

    updateComfort();
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

// Thumbstick locomotion. Running or walking as with the keyboard: running (cl_alwaysrun, the
// default; the speed button switches) is cl_movespeedkey times the speed, and the stick, being
// analog, moves at cl_forwardspeed in every direction (as the old engine did). The server steers by
// the head (.v_viewangle): with
// vr_movement_mode 1 the stick moves relative to the head; with 0 it moves where the moving hand
// (hands::moveHand) points, with 2 the left hand, 3 the right hand, expressed relative to the head. Either way, pointing it up or down while
// pushing forward swims up or down (from the old engine's VR_Move).
extern "C" void VR_AdjustMove(float* forwardmove, float* sidemove, float* upmove)
{
    if(moveAxes == glm::vec2{0.f})
    {
        return;
    }

    const bool running = ((in_speed.state & 1) != 0) != (cl_alwaysrun.value != 0.f);
    const float speedScale = running ? cl_movespeedkey.value : 1.f;
    const hands::State& s = hands::current();

    float fwd = moveAxes.y;
    float side = moveAxes.x;

    const int mode = static_cast<int>(vr_movement_mode.value);
    if(s.valid && mode != 1)
    {
        // 2 the left hand, 3 the right hand; 0 the moving stick's.
        const int hand = mode == 2 ? HAND_OFF : mode == 3 ? HAND_MAIN : hands::moveHand();
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
        const glm::vec3 move = (moveAxes.y * lfwd + moveAxes.x * lright) * fac;

        glm::vec3 vfwd, vright, vup;
        hands::angleVectors({0.f, s.headAngles.y, 0.f}, vfwd, vright, vup);
        fwd = glm::dot(move, vfwd);
        side = glm::dot(move, vright);
    }

    *forwardmove += fwd * cl_forwardspeed.value * speedScale;
    *sidemove += side * cl_forwardspeed.value * speedScale;

    if(s.valid)
    {
        *upmove += cl_upspeed.value * moveAxes.y * hands::forward(s.rot[hands::moveHand()]).z * speedScale;
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
    const bool rising = vr_roomscale_jump.value && vrActive() && s.valid && key_dest == key_game && !posing::active() &&
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
