// vr_inputlag.cpp -- vr_inputlag_test: how many frames (and ms) a desktop key or mouse motion takes to reach the
// game. The input is pushed into SDL's event queue (as the OS would), so it goes the whole way: SDL's event pump
// (IN_SendKeyEvents), the key's binding (+forward in the command buffer) or the mouse's accumulation, the move sent
// (CL_SendCmd), the server's client command, the player's velocity, and the view drawn. Each stage's first frame is
// printed (frames after the one the event was pushed in, and the wall clock since). Flat (vr_enabled 0) and VR.
//
//   vr_inputlag_test [key|mouse|turn] [settle frames] [command run when done ...]
//
// Run it with nothing else in the command buffer (a script's later "wait"s would hold the key's +forward back):
// `vr_inputlag_test key 60 vr_inputlag_test mouse 60 toggleconsole / quit` chains the tests ("/": a ";").
//
// key:   the W key (scancode, as the keyboard sends it) held until the player is at 90% of his speed, then let go:
//        the stages above, then the release's (-forward, and the player down to 10% of the speed).
// turn:  the key and a 1000-count mouse motion together (the walk's direction: where the view looks, how soon).
// mouse: a 100-count mouse motion to the right (relative): the client's yaw, the server's v_angle, the view's.
//
// The event goes into SDL's queue, so what happens before it gets there (Windows' keyboard hooks, the message queue)
// isn't timed. vr_keyhook_status prints whether this copy holds Ironwail's desktop keyboard hook (WH_KEYBOARD_LL) and
// the longest the main thread left it unserviced since the last report: every key press on the desktop, in any
// program, waits for it (ROUND21.md, "Flat-screen input lag").

#include "vr_engine.hpp"
#include "vr_input.hpp"
#include "vr_main.hpp"

#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <math.h>

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include <string.h>

extern "C" kbutton_t in_forward; // cl_input.c
extern "C" float host_netinterval; // host.c
extern "C" sizebuf_t cmd_text;     // cmd.c
extern "C" qboolean cmd_wait;      // cmd.c
extern "C" cvar_t cl_movespeedkey, sv_maxspeed; // cl_input.c, sv_user.c

namespace qvr::inputlag
{

namespace
{

enum class Phase
{
    Off,
    Settle,
    Measure,
};

enum Stage
{
    StageInput,  // the key's +forward ran (in_forward held) / the client's yaw turned
    StageCmd,    // the move sent carries it (cl.cmd.forwardmove) / the server's client's view angle turned
    StageServer, // the server's client command has it (forwardmove)
    StageMoving, // the server's player moves (velocity) / -
    StageView,   // the view drawn moved (r_refdef.vieworg) / turned (r_refdef.viewangles)
    StageAlong,  // the player moving (over 50 units/s) within 5 degrees of where the view looks
    StageFull,   // the player at 90% of his speed (the key's: forwardmove, at most sv_maxspeed)
    StageUp,     // (the key let go at full speed) the key's -forward ran
    StageStop,   // the player down to 10% of that speed
    StageCount,
};

constexpr const char* keyStageNames[StageCount] = {"key bound command ran (in_forward)", "client move sent (cl.cmd)",
    "server client cmd", "server player velocity", "view origin moved",
    "moving where the view looks", "player at 90% speed", "key up: -forward ran", "key up: player below 10% speed"};
constexpr const char* mouseStageNames[StageCount] = {"client yaw (cl.viewangles)", "server v_angle yaw", "-", "-",
    "view yaw (r_refdef)", "-", "-", "-", "-"};

constexpr int timeoutFrames = 2000;
constexpr double timeoutSeconds = 3.0;

struct Test
{
    Phase phase = Phase::Off;
    bool mouse = false;
    bool turn = false;        // (turn) the mouse turned and the key pressed in the same frame
    int settle = 0;           // frames left to wait before pushing the event
    int frame0 = 0;           // host_framecount when the event was pushed
    double time0 = 0.0;       // Sys_DoubleTime then
    int stageFrame[StageCount] = {};
    double stageTime[StageCount] = {};
    vec3_t view0 = {};        // r_refdef.vieworg before
    float yaw0 = 0.f;         // cl.viewangles[YAW] before
    float svYaw0 = 0.f;       // the server player's v_angle[YAW] before
    float viewYaw0 = 0.f;     // r_refdef.viewangles[YAW] before
    bool keyHeld = false;
    int upFrame = 0;          // host_framecount when the key was let go
    double upTime = 0.0;
    int groundFrames[2] = {}; // the frames measured off the ground and on it (FL_ONGROUND: friction)
    float gapServer = 0.f;    // the view's largest distance (horizontal) behind the server's player
    float gapClient = 0.f;    // and behind the client's player entity (the lerped origin the view starts from)
    char then[256] = {};      // the commands run when it is done (the test's own script: none waits in the buffer)
};

Test test;

[[nodiscard]] edict_t* serverPlayer()
{
    return sv.active && svs.maxclients >= 1 && svs.clients[0].active ? svs.clients[0].edict : nullptr;
}

[[nodiscard]] float yawDelta(float a, float b)
{
    float d = a - b;
    while(d > 180.f)
    {
        d -= 360.f;
    }
    while(d < -180.f)
    {
        d += 360.f;
    }
    return d;
}

void pushKey(bool down)
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.scancode = SDL_SCANCODE_W;
    e.key.keysym.sym = SDLK_w;
    e.key.timestamp = SDL_GetTicks();
    SDL_PushEvent(&e);
    test.keyHeld = down;
}

void pushMouse()
{
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEMOTION;
    e.motion.xrel = test.turn ? 1000 : 100;
    e.motion.timestamp = SDL_GetTicks();
    SDL_PushEvent(&e);
}

// The desktop keyboard hook (sys_sdl_win.c's WH_KEYBOARD_LL, Ironwail's: Caps Lock, Print Screen... as game keys):
// whether this copy holds it, and the longest the main thread went without servicing it since the last report (each
// key press on the desktop, in any program, waits that long for it, up to Windows' LowLevelHooksTimeout).
void keyHookStatus()
{
    qboolean active = false;
    double gap = 0.0;
    Sys_KeyFilterStats(&active, &gap, true);
    const bool focus = (SDL_GetWindowFlags(static_cast<SDL_Window*>(VID_GetWindow())) & SDL_WINDOW_INPUT_FOCUS) != 0;
    Con_Printf("inputlag: keyboard hook %s (window %s); longest unserviced %.0f ms\n", active ? "installed" : "not installed",
        focus ? "focused" : "not focused", gap * 1000.0);
}

void hookStatus_f()
{
    keyHookStatus();
}

void start()
{
    edict_t* p = serverPlayer();
    VectorCopy(r_refdef.vieworg, test.view0);
    test.yaw0 = cl.viewangles[YAW];
    test.svYaw0 = p ? p->v.v_angle[YAW] : 0.f;
    test.viewYaw0 = r_refdef.viewangles[YAW];
    for(int i = 0; i < StageCount; i++)
    {
        test.stageFrame[i] = -1;
    }
    test.frame0 = host_framecount;
    test.time0 = Sys_DoubleTime();
    test.phase = Phase::Measure;
    Con_Printf("inputlag: %s pushed at frame %d (vr %s, key_dest %d, host_maxfps %g, vsync %g, netinterval %.4f)\n",
        test.mouse ? "mouse motion" : test.turn ? "W key down and a turn" : "W key down", test.frame0, VR_IsActive() ? "on" : "off", static_cast<int>(key_dest),
        Cvar_VariableValue("host_maxfps"), Cvar_VariableValue("vid_vsync"), host_netinterval);
    // What waits in the command buffer: a key's command (+forward) runs only after it.
    Con_Printf("inputlag: command buffer %d bytes%s\n", cmd_text.cursize, cmd_wait ? ", waiting" : "");
    keyHookStatus();
    if(test.mouse)
    {
        pushMouse();
    }
    else
    {
        pushKey(true);
        if(test.turn)
        {
            pushMouse();
        }
    }
}

void mark(Stage s, bool reached)
{
    if(reached && test.stageFrame[s] < 0)
    {
        test.stageFrame[s] = host_framecount - test.frame0;
        test.stageTime[s] = (Sys_DoubleTime() - test.time0) * 1000.0;
    }
}

void finish(bool timedOut)
{
    const char* const* names = test.mouse ? mouseStageNames : keyStageNames;
    const int frames = host_framecount - test.frame0;
    const double ms = (Sys_DoubleTime() - test.time0) * 1000.0;
    Con_Printf("inputlag: %s after %d frames, %.1f ms (%.2f ms a frame)\n", timedOut ? "timed out" : "done", frames, ms,
        frames > 0 ? ms / frames : 0.0);
    if(!test.mouse)
    {
        Con_Printf("inputlag:   on the ground %d frames, off it %d (sv_friction %g, sv_stopspeed %g, sv_accelerate %g)\n",
            test.groundFrames[1], test.groundFrames[0], Cvar_VariableValue("sv_friction"),
            Cvar_VariableValue("sv_stopspeed"), Cvar_VariableValue("sv_accelerate"));
        Con_Printf("inputlag:   view behind the player at most %.1f units (the server's), %.1f (the client's entity)\n",
            test.gapServer, test.gapClient);
    }
    for(int i = 0; i < StageCount; i++)
    {
        if(names[i][0] == '-')
        {
            continue;
        }
        if(test.stageFrame[i] < 0)
        {
            Con_Printf("inputlag:   %-36s never\n", names[i]);
        }
        else
        {
            Con_Printf("inputlag:   %-36s frame +%d, %.1f ms%s\n", names[i], test.stageFrame[i], test.stageTime[i],
                i >= StageUp ? " (after the key up)" : "");
        }
    }
    if(test.keyHeld)
    {
        pushKey(false);
    }
    test.phase = Phase::Off;
    if(test.then[0])
    {
        // ("/" between commands: a script's ";"; kept for a chained test, which passes the rest on.)
        if(strncmp(test.then, "vr_inputlag_test ", 17) != 0)
        {
            for(char* c = test.then; *c; c++)
            {
                *c = *c == '/' ? ';' : *c;
            }
        }
        Cbuf_AddText(test.then);
        Cbuf_AddText("\n");
    }
}

void measure()
{
    edict_t* p = serverPlayer();
    if(test.mouse)
    {
        mark(StageInput, za::fabs(yawDelta(cl.viewangles[YAW], test.yaw0)) > 0.01f);
        mark(StageCmd, p && za::fabs(yawDelta(p->v.v_angle[YAW], test.svYaw0)) > 0.01f);
        mark(StageView, za::fabs(yawDelta(r_refdef.viewangles[YAW], test.viewYaw0)) > 0.01f);
    }
    else
    {
        mark(StageInput, (in_forward.state & 1) != 0);
        mark(StageCmd, cl.cmd.forwardmove != 0.f);
        mark(StageServer, sv.active && svs.clients[0].cmd.forwardmove != 0.f);
        mark(StageMoving, p && (p->v.velocity[0] != 0.f || p->v.velocity[1] != 0.f));
        mark(StageView, za::fabs(r_refdef.vieworg[0] - test.view0[0]) + za::fabs(r_refdef.vieworg[1] - test.view0[1]) > 0.01f);
        if(p)
        {
            test.groundFrames[(static_cast<int>(p->v.flags) & FL_ONGROUND) != 0]++;
            test.gapServer = za::max(test.gapServer, za::fabs(r_refdef.vieworg[0] - p->v.origin[0]) +
                                                         za::fabs(r_refdef.vieworg[1] - p->v.origin[1]));
        }
        if(cl.viewentity > 0 && cl.viewentity < cl.num_entities && cl_entities)
        {
            const entity_t& e = cl_entities[cl.viewentity];
            test.gapClient = za::max(test.gapClient,
                za::fabs(r_refdef.vieworg[0] - e.origin[0]) + za::fabs(r_refdef.vieworg[1] - e.origin[1]));
        }
        const float speed = p ? sqrtf(p->v.velocity[0] * p->v.velocity[0] + p->v.velocity[1] * p->v.velocity[1]) : 0.f;
        const float target =
            za::min(cl_forwardspeed.value * (cl_alwaysrun.value != 0.f ? cl_movespeedkey.value : 1.f), sv_maxspeed.value);
        if(speed > 50.f)
        {
            const float moveYaw = atan2f(p->v.velocity[1], p->v.velocity[0]) * 180.f / static_cast<float>(M_PI);
            mark(StageAlong, za::fabs(yawDelta(moveYaw, cl.viewangles[YAW])) < 5.f);
        }
        if(test.keyHeld)
        {
            mark(StageFull, speed >= 0.9f * target);
            if(test.stageFrame[StageFull] >= 0)
            {
                // Let go: the release's latency, measured from now.
                pushKey(false);
                test.upFrame = host_framecount;
                test.upTime = Sys_DoubleTime();
            }
        }
        else
        {
            if((in_forward.state & 1) == 0 && test.stageFrame[StageUp] < 0)
            {
                test.stageFrame[StageUp] = host_framecount - test.upFrame;
                test.stageTime[StageUp] = (Sys_DoubleTime() - test.upTime) * 1000.0;
            }
            if(speed <= 0.1f * target && test.stageFrame[StageStop] < 0)
            {
                test.stageFrame[StageStop] = host_framecount - test.upFrame;
                test.stageTime[StageStop] = (Sys_DoubleTime() - test.upTime) * 1000.0;
            }
        }
    }
    const int frames = host_framecount - test.frame0;
    const bool timedOut = frames > timeoutFrames || Sys_DoubleTime() - test.time0 > timeoutSeconds;
    bool all = true; // (each stage that applies: the mouse's has no client command or velocity)
    for(int i = 0; i < StageCount; i++)
    {
        all = all && (test.stageFrame[i] >= 0 || (test.mouse && i != StageInput && i != StageCmd && i != StageView));
    }
    if(all || timedOut)
    {
        finish(timedOut);
    }
}

void test_f()
{
    if(cls.state != ca_connected || cls.signon != SIGNONS)
    {
        Con_Printf("vr_inputlag_test: load a map first\n");
        return;
    }
    test = Test{};
    test.mouse = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "mouse");
    test.turn = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "turn");
    test.settle = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 30;
    test.phase = Phase::Settle;
    for(int i = 3; i < Cmd_Argc(); i++)
    {
        const char* a = Cmd_Argv(i);
        q_strlcat(test.then, a, sizeof(test.then));
        q_strlcat(test.then, " ", sizeof(test.then));
    }
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_inputlag_test", test_f);
    Cmd_AddCommand("vr_keyhook_status", hookStatus_f);
}

void frameEnd()
{
    switch(test.phase)
    {
        case Phase::Off: return;
        case Phase::Settle:
            // (The command ran from the console: the game's keys only once it is down, key_game.)
            if(key_dest == key_game && test.settle-- <= 0)
            {
                start();
            }
            return;
        case Phase::Measure: measure(); return;
    }
}

} // namespace qvr::inputlag
