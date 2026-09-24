// vr_input.cpp -- controller input: commands, locomotion, turning, menu navigation, haptics.
//
// Controller buttons run the same commands as keys would (+attack, +grabright, impulse 10...),
// so nothing depends on key bindings in a config file.

#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_input.hpp"
#include "vr_main.hpp"
#include "vr_protocol.hpp"

#include <vector>

using namespace qvr;
using namespace qvr::protocol;

namespace
{

InputState previous;
bool previousWasGame = false;
glm::vec2 moveAxes{0.f};
bool snapTurnArmed = true;
bool menuStickArmed[2] = {true, true};

struct PendingHaptic
{
    double time;
    int hand;
    float seconds;
    float frequency;
    float amplitude;
};

std::vector<PendingHaptic> pendingHaptics;

void command(const char* text)
{
    Cbuf_AddText(text);
}

// Runs "+name" on press and "-name" on release.
void button(bool now, bool before, const char* name)
{
    if(now != before)
    {
        command(va("%c%s\n", now ? '+' : '-', name));
    }
}

void key(bool now, bool before, int k)
{
    if(now != before)
    {
        Key_Event(k, now);
    }
}

[[nodiscard]] float deadzone(float v)
{
    const float dz = CLAMP(0.f, vr_deadzone.value / 100.f, 0.9f);
    if(std::fabs(v) < dz)
    {
        return 0.f;
    }
    return (v - std::copysign(dz, v)) / (1.f - dz);
}

// Releases the game buttons held, when a menu opens.
void releaseGameButtons()
{
    const InputState& p = previous;
    const bool left = !vr_lefthanded.value;

    button(false, p.fire[HAND_MAIN], "attack");
    button(false, p.fire[HAND_OFF], "offhandattack");
    button(false, p.grab[HAND_OFF], left ? "grableft" : "grabright");
    button(false, p.grab[HAND_MAIN], left ? "grabright" : "grableft");
    button(false, p.reload[HAND_OFF], left ? "reloadleft" : "reloadright");
    button(false, p.reload[HAND_MAIN], left ? "reloadright" : "reloadleft");
    button(false, p.jump, "jump");
}

void gameInput(const InputState& in)
{
    const InputState& p = previous;
    const bool leftIsOff = !vr_lefthanded.value;

    button(in.fire[HAND_MAIN], p.fire[HAND_MAIN], "attack");
    button(in.fire[HAND_OFF], p.fire[HAND_OFF], "offhandattack");
    button(in.grab[HAND_OFF], p.grab[HAND_OFF], leftIsOff ? "grableft" : "grabright");
    button(in.grab[HAND_MAIN], p.grab[HAND_MAIN], leftIsOff ? "grabright" : "grableft");
    button(in.reload[HAND_OFF], p.reload[HAND_OFF], leftIsOff ? "reloadleft" : "reloadright");
    button(in.reload[HAND_MAIN], p.reload[HAND_MAIN], leftIsOff ? "reloadright" : "reloadleft");
    button(in.jump, p.jump, "jump");

    if(in.nextWeapon[HAND_MAIN] && !p.nextWeapon[HAND_MAIN])
    {
        command("impulse 10\n");
    }
    if(in.nextWeapon[HAND_OFF] && !p.nextWeapon[HAND_OFF])
    {
        command("impulse 12\n");
    }

    moveAxes = {deadzone(in.move.x), deadzone(in.move.y)};

    // Turning: snap by vr_snap_turn degrees, or smooth at vr_turn_speed.
    const float turn = deadzone(in.turn.x);
    if(vr_enable_joystick_turn.value)
    {
        if(vr_snap_turn.value > 0.f)
        {
            if(std::fabs(turn) < 0.3f)
            {
                snapTurnArmed = true;
            }
            else if(snapTurnArmed && std::fabs(turn) > 0.7f)
            {
                snapTurnArmed = false;
                hands::addTurn(turn > 0.f ? -vr_snap_turn.value : vr_snap_turn.value);
            }
        }
        else
        {
            hands::addTurn(-turn * static_cast<float>(host_frametime) * 100.f * vr_turn_speed.value);
        }
    }
}

void menuInput(const InputState& in)
{
    const InputState& p = previous;

    // Stick flicks navigate; the main trigger or jump button confirms.
    const glm::vec2 stick = in.move + in.turn;
    const auto flick = [&](int axis, float value, int negKey, int posKey) {
        if(std::fabs(value) < 0.3f)
        {
            menuStickArmed[axis] = true;
        }
        else if(menuStickArmed[axis] && std::fabs(value) > 0.7f)
        {
            menuStickArmed[axis] = false;
            Key_Event(value > 0.f ? posKey : negKey, true);
            Key_Event(value > 0.f ? posKey : negKey, false);
        }
    };
    flick(0, stick.x, K_LEFTARROW, K_RIGHTARROW);
    flick(1, stick.y, K_DOWNARROW, K_UPARROW);

    key(in.fire[HAND_MAIN] || in.jump, p.fire[HAND_MAIN] || p.jump, K_ENTER);
    moveAxes = glm::vec2{0.f};
}

} // namespace

namespace qvr::input
{

void update(const InputState& in)
{
    if(!vrActive())
    {
        moveAxes = glm::vec2{0.f};
        return;
    }

    // The menu button toggles the menu, like Escape.
    if(in.menu && !previous.menu)
    {
        Key_Event(K_ESCAPE, true);
        Key_Event(K_ESCAPE, false);
    }

    const bool game = key_dest == key_game;
    if(game)
    {
        if(!previousWasGame)
        {
            // Buttons still held from the menu must not act in the game.
            previous = in;
        }
        gameInput(in);
    }
    else
    {
        if(previousWasGame)
        {
            releaseGameButtons();
        }
        menuInput(in);
    }

    previous = in;
    previousWasGame = game;

    // Delayed haptics (see VR_ParseHaptic).
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

} // namespace qvr::input

// Thumbstick locomotion, relative to the head (the server steers by .v_viewangle).
extern "C" void VR_AdjustMove(float* forwardmove, float* sidemove)
{
    if(moveAxes == glm::vec2{0.f})
    {
        return;
    }

    const float speedScale = (in_speed.state & 1) ? cl_movespeedkey.value : 1.f;
    *forwardmove += moveAxes.y * (moveAxes.y > 0.f ? cl_forwardspeed.value : cl_backspeed.value) * speedScale;
    *sidemove += moveAxes.x * cl_sidespeed.value * speedScale;
}

// Server side: `haptic(hand, delay, duration, frequency, amplitude)` from QC, sent to the
// player the builtin was called for.
extern "C" void VR_SendHaptic(edict_t* player, int hand, float delay, float duration, float frequency, float amplitude)
{
    const int client = NUM_FOR_EDICT(player) - 1;
    if(client < 0 || client >= svs.maxclients || !svs.clients[client].active)
    {
        return;
    }

    sizebuf_t* msg = &svs.clients[client].message;
    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, QVR_SVC_HAPTIC);
    MSG_WriteByte(msg, hand);
    MSG_WriteFloat(msg, delay);
    MSG_WriteFloat(msg, duration);
    MSG_WriteFloat(msg, frequency);
    MSG_WriteFloat(msg, amplitude);
}

void VR_ParseHaptic()
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
