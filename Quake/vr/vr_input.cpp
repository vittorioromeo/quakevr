// vr_input.cpp -- controller input: keys, locomotion, turning, haptics.
//
// Controller buttons are Quake keys, so everything they do comes from bindings (defaults in
// quakevr/default.cfg) and can be rebound -- with aliases -- from the console or the bindings
// menu. They reuse Ironwail's gamepad keys by role, not by side, so that vr_lefthanded needs
// no rebinding: the main hand is the "right" half of a gamepad (RT, RB, A, B, RS), the off
// hand the "left" half (LT, LB, X, Y, LS). Menus understand these keys already.
//
// The off hand's stick moves (analog, see VR_AdjustMove); the main hand's stick turns, and
// pushed up or down it is DPAD UP/DOWN. In menus both sticks are the DPAD.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_input.hpp"
#include "vr_main.hpp"
#include "vr_menuui.hpp"
#include "vr_voicenotes.hpp"
#include "vr_flashlight.hpp"

#include <utility>
#include <vector>

using namespace qvr;

namespace
{

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

// A stick direction acting as a key: pressed past 0.7, released below 0.5, auto-repeating in
// menus.
struct StickKey
{
    int key;
    bool down{false};
    double nextRepeat{0.0};
};

StickKey stickKeys[] = {{K_DPAD_UP}, {K_DPAD_DOWN}, {K_DPAD_LEFT}, {K_DPAD_RIGHT}};

InputState previous;
glm::vec2 moveAxes{0.f};
bool snapTurnArmed = true;

struct PendingHaptic
{
    double time;
    int hand;
    float seconds;
    float frequency;
    float amplitude;
};

std::vector<PendingHaptic> pendingHaptics;

[[nodiscard]] float deadzone(float v)
{
    const float dz = CLAMP(0.f, vr_deadzone.value / 100.f, 0.9f);
    if(std::fabs(v) < dz)
    {
        return 0.f;
    }
    return (v - std::copysign(dz, v)) / (1.f - dz);
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
        if(std::fabs(x) < 0.3f)
        {
            snapTurnArmed = true;
        }
        else if(snapTurnArmed && std::fabs(x) > 0.7f)
        {
            snapTurnArmed = false;
            hands::addTurn(x > 0.f ? -vr_snap_turn.value : vr_snap_turn.value);
        }
    }
    else if(const float v = deadzone(x); v != 0.f)
    {
        hands::addTurn(-v * static_cast<float>(host_frametime) * 100.f * vr_turn_speed.value);
    }
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

void update(const InputState& tracked)
{
    // Without VR, everything is released (keys held when the session ends must come up).
    static const InputState released;
    const bool active = vrActive();
    const InputState& in = active ? tracked : released;

    const HandInput& off = in.hands[HAND_OFF];
    const HandInput& main = in.hands[HAND_MAIN];

    // Where the hands point at the menu, before the trigger clicks there.
    menuui::update(hands::current());

    for(int h = 0; h < HAND_COUNT; h++)
    {
        for(const ButtonKeys& b : buttonKeys)
        {
            const bool now = in.hands[h].*b.button;
            if(now != previous.hands[h].*b.button)
            {
                // The off hand's upper button at the mouth records a voice note instead.
                if(h == HAND_OFF && b.button == &HandInput::secondary && voicenotes::offhandButton(now))
                {
                    continue;
                }
                // A hand at the chest flashlight switches it (trigger) or takes it (grip) instead.
                if((b.button == &HandInput::trigger || b.button == &HandInput::grip) &&
                    flashlight::button(h, b.button == &HandInput::grip, now))
                {
                    continue;
                }
                // A trigger pointing at the menu is its mouse button.
                const int key = b.button == &HandInput::trigger ? menuui::triggerKey(h, now, b.key[h]) : b.key[h];
                Key_Event(key, now);
                if(now && key_dest == key_menu && !vr_disablehaptics.value)
                {
                    // A click under the finger, as the old engine gave in menus.
                    pendingHaptics.push_back({realtime, h, 0.02f, 150.f, 0.3f});
                }
            }
        }

        // The menu button is Escape: it opens and closes the menu and can never be unbound.
        if(in.hands[h].menu && !previous.hands[h].menu)
        {
            Key_Event(K_ESCAPE, true);
            Key_Event(K_ESCAPE, false);
        }
    }

    const bool menu = key_dest != key_game;
    if(menu)
    {
        const glm::vec2 stick = off.stick + main.stick;
        stickKey(stickKeys[0], stick.y, true);
        stickKey(stickKeys[1], -stick.y, true);
        stickKey(stickKeys[2], -stick.x, true);
        stickKey(stickKeys[3], stick.x, true);
        moveAxes = glm::vec2{0.f};
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

    previous = in;
    runHaptics();
}

} // namespace qvr::input

// Thumbstick locomotion. Running or walking as with the keyboard: running (cl_alwaysrun, the
// default; the speed button switches) is cl_movespeedkey times the speed, and the stick, being
// analog, moves at cl_forwardspeed in every direction (as the old engine did). The server steers by
// the head (.v_viewangle): with
// vr_movement_mode 1 the stick moves relative to the head; with 0 it moves where the off hand
// points, expressed relative to the head. Either way, pointing the off hand up or down while
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

    if(s.valid && static_cast<int>(vr_movement_mode.value) == 0)
    {
        glm::vec3 lfwd, lright, lup;
        hands::angleVectors(s.rot[HAND_OFF], lfwd, lright, lup);

        // Pointing (nearly) straight up or down: steer with the hand's up vector instead.
        if(std::fabs(lfwd.z) > 0.8f)
        {
            if(lfwd.z < -0.8f)
            {
                lfwd = -lfwd;
            }
            else
            {
                lup = -lup;
            }
            std::swap(lup, lfwd);
        }

        // Tilting the hand must not change the speed.
        const float fac = 1.f / std::fmax(std::fabs(lup.z), 0.2f);
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
        *upmove += cl_upspeed.value * moveAxes.y * hands::forward(s.rot[HAND_OFF]).z * speedScale;
    }
}

namespace qvr::input
{

void roomscaleJump(const hands::State& s)
{
    static bool jumping = false;
    const bool rising = vr_roomscale_jump.value && vrActive() && s.valid && key_dest == key_game &&
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

    if(hand == HAND_OFF || hand == HAND_MAIN)
    {
        pendingHaptics.push_back({realtime + delay, hand, duration, frequency, amplitude});
    }
}

} // namespace qvr::input
