// vr_timescale.cpp -- see vr_timescale.hpp.

#include "vr_timescale.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"

#include <glm/gtc/quaternion.hpp>

extern "C"
{
double vr_gametime = 0.0; // realtime, slowed by the time scale (vr_api.h)
}

namespace qvr::timescale
{
namespace
{

constexpr float minScale = 0.05f;

// The time scale in effect, eased towards vr_timescale's; and how far vr_gametime has fallen behind realtime.
struct Clock
{
    float scale = 1.f;
    double behind = 0.0;
};
Clock scaleClock;

// A hand as the slowed world has it (tracking space, metres), while it follows its controller.
struct Follow
{
    glm::vec3 pos{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    bool on = false;
};

// The hands' followers, and the runtime clock slowed with the world (filterHands).
struct Hands
{
    Follow hand[HAND_COUNT];
    double behind = 0.0;       // seconds the slowed runtime clock is behind the runtime's
    double lastRuntime = -1.0; // the runtime clock at the last frame (< 0: none yet)
    float lag[HAND_COUNT]{};   // metres each hand is behind its controller (vr_slowmo_probe)
};
Hands slowHands;

// The scale vr_timescale asks for: only for a local single-player game (a remote server keeps its own time).
[[nodiscard]] float wanted()
{
    if(!sv.active || svs.maxclients != 1 || vr_timescale.value <= 0.f || vr_timescale.value == 1.f)
    {
        return 1.f;
    }
    return za::clamp(vr_timescale.value, minScale, 4.f);
}

// vr_slowmo [scale]: toggles slow motion (vr_timescale between 1 and vr_slowmo_scale); with a scale, sets that.
void slowmo_f()
{
    if(Cmd_Argc() >= 2)
    {
        Cvar_SetValueQuick(&vr_timescale, static_cast<float>(Q_atof(Cmd_Argv(1))));
    }
    else
    {
        Cvar_SetValueQuick(&vr_timescale, vr_timescale.value != 1.f ? 1.f : vr_slowmo_scale.value);
    }
    Con_Printf("time scale %g%s\n", vr_timescale.value,
               wanted() == 1.f && vr_timescale.value != 1.f ? " (single player only)" : "");
}

// vr_slowmo_probe [classname | number]: one line of the clocks (the server's, realtime, vr_gametime, the client's) and the
// player's origin and velocity; with a classname, the newest such entity's too (or that entity's). Slow motion's tests compare these at
// the same game time at different scales (ROUND21.md, "Slow motion").
void probe_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_slowmo_probe: no local game\n");
        return;
    }
    qcvm_t* const old = qcvm;
    if(old != &sv.qcvm)
    {
        if(old)
        {
            PR_SwitchQCVM(nullptr);
        }
        PR_SwitchQCVM(&sv.qcvm);
    }
    const edict_t* const pl = sv_player;
    Con_Printf("probe scale %.3f sv %.4f real %.4f game %.4f cl %.4f player %.2f %.2f %.2f vel %.1f %.1f %.1f", scaleClock.scale,
               qcvm->time, realtime, vr_gametime, cl.time, pl ? pl->v.origin[0] : 0.f, pl ? pl->v.origin[1] : 0.f,
               pl ? pl->v.origin[2] : 0.f, pl ? pl->v.velocity[0] : 0.f, pl ? pl->v.velocity[1] : 0.f,
               pl ? pl->v.velocity[2] : 0.f);
    {
        const hands::State& hs = hands::current();
        Con_Printf(" | main hand %.2f m/s, %.3f m behind", glm::length(hs.vel[HAND_MAIN]), slowHands.lag[HAND_MAIN]);
    }
    if(Cmd_Argc() >= 2)
    {
        edict_t* found = nullptr;
        const int number = Q_atoi(Cmd_Argv(1));
        if(number > 0 && number < qcvm->num_edicts && !EDICT_NUM(number)->free)
        {
            found = EDICT_NUM(number);
        }
        for(int i = 1; number <= 0 && i < qcvm->num_edicts; i++)
        {
            edict_t* const e = EDICT_NUM(i);
            if(!e->free && !strcmp(PR_GetString(e->v.classname), Cmd_Argv(1)))
            {
                found = e; // (the newest: the loop goes on past it)
            }
        }
        if(found)
        {
            Con_Printf(" | %s %d next %.4f at %.2f %.2f %.2f vel %.1f %.1f %.1f", Cmd_Argv(1), NUM_FOR_EDICT(found), found->v.nextthink, found->v.origin[0],
                       found->v.origin[1], found->v.origin[2], found->v.velocity[0], found->v.velocity[1],
                       found->v.velocity[2]);
        }
        else
        {
            Con_Printf(" | no %s", Cmd_Argv(1));
        }
    }
    Con_Printf("\n");
    if(old != &sv.qcvm)
    {
        PR_SwitchQCVM(nullptr);
        if(old)
        {
            PR_SwitchQCVM(old);
        }
    }
}

// What is left of the turn from `from` to `to`: its angle (radians, 0 .. pi) and axis.
[[nodiscard]] float turnBetween(const glm::quat& from, const glm::quat& to, glm::vec3& axis)
{
    glm::quat d = to * glm::inverse(from);
    if(d.w < 0.f)
    {
        d = -d;
    }
    const glm::vec3 v{d.x, d.y, d.z};
    const float s = glm::length(v);
    if(s < 1e-7f)
    {
        axis = glm::vec3{0.f, 0.f, 1.f};
        return 0.f;
    }
    axis = v / s;
    return 2.f * za::atan2(s, d.w);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_slowmo", slowmo_f);
    Cmd_AddCommand("vr_slowmo_probe", probe_f);
}

float current()
{
    return scaleClock.scale;
}

void filterHands(TrackingState& t)
{
    const float s = scaleClock.scale;

    // The runtime clock, slowed with the world: what the throws' and spins' velocities are measured on.
    if(t.time >= 0.0)
    {
        if(slowHands.lastRuntime >= 0.0 && t.time > slowHands.lastRuntime && s != 1.f)
        {
            slowHands.behind += (t.time - slowHands.lastRuntime) * (1.0 - static_cast<double>(s));
        }
        slowHands.lastRuntime = t.time;
        if(slowHands.behind != 0.0)
        {
            t.time -= slowHands.behind;
        }
    }

    const bool inGame = key_dest == key_game && cls.state == ca_connected;
    bool following = false;
    for(const Follow& f : slowHands.hand)
    {
        following = following || f.on;
    }
    if(!inGame || (s == 1.f && !following))
    {
        for(int h = 0; h < HAND_COUNT; h++)
        {
            slowHands.hand[h].on = false;
            slowHands.lag[h] = 0.f;
        }
        return;
    }

    const float dt = static_cast<float>(host_frametime); // the game's time this frame (slowed)
    const float maxStep = vr_timescale_hand_speed.value > 0.f ? vr_timescale_hand_speed.value * dt : 1e9f;
    const float maxTurn = vr_timescale_hand_spin.value > 0.f ? vr_timescale_hand_spin.value * dt : 1e9f;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        Pose& p = t.hands[h];
        Follow& f = slowHands.hand[h];
        if(!p.valid)
        {
            f.on = false;
            continue;
        }
        if(!f.on)
        {
            f.pos = p.position;
            f.rot = p.orientation;
            f.on = true;
        }

        // Towards the controller, as fast as the slowed world lets it.
        const glm::vec3 d = p.position - f.pos;
        const float len = glm::length(d);
        const bool limited = len > maxStep;
        const glm::vec3 step = limited ? d * (maxStep / len) : d;
        f.pos += step;

        glm::vec3 axis;
        const float angle = turnBetween(f.rot, p.orientation, axis);
        const bool turnLimited = angle > maxTurn;
        f.rot = turnLimited ? glm::normalize(glm::slerp(f.rot, p.orientation, maxTurn / angle)) : p.orientation;

        // Its velocities in the game's time: the controller's, sped up as the world is slowed (a hand moved slowly
        // is a normal blow in the slowed world), or the most the follower moves.
        if(p.velocityValid)
        {
            const float toGame = 1.f / s;
            const glm::vec3 moved = dt > 0.f ? step / dt : glm::vec3{0.f};
            p.linearVelocity = limited ? moved : p.linearVelocity * toGame;
            p.gripVelocity = limited ? moved : p.gripVelocity * toGame;
            p.angularVelocity = turnLimited ? axis * (dt > 0.f ? maxTurn / dt : 0.f) : p.angularVelocity * toGame;
        }
        slowHands.lag[h] = glm::length(p.position - f.pos);
        p.position = f.pos;
        p.orientation = f.rot;

        // Back at a time scale of 1 and caught up: the controller's own pose from the next frame on.
        if(s == 1.f && !limited && !turnLimited)
        {
            f.on = false;
        }
    }
}

} // namespace qvr::timescale

// Host_AdvanceTime, after realtime advanced by dt: the time scale eased towards vr_timescale's (over
// vr_timescale_ramp seconds for the whole way from 1), and vr_gametime.
extern "C" void VR_AdvanceTime(double dt)
{
    using qvr::timescale::scaleClock;
    const float target = qvr::timescale::wanted();
    if(scaleClock.scale != target)
    {
        const float ramp = qvr::vr_timescale_ramp.value;
        const float most = ramp > 0.f ? static_cast<float>(dt) / ramp : 1.f;
        const float gap = target - scaleClock.scale;
        scaleClock.scale = za::fabs(gap) <= most ? target : scaleClock.scale + (gap > 0.f ? most : -most);
    }
    if(scaleClock.scale != 1.f)
    {
        scaleClock.behind += dt * (1.0 - static_cast<double>(scaleClock.scale));
    }
    vr_gametime = scaleClock.behind != 0.0 ? realtime - scaleClock.behind : realtime;
}

extern "C" double VR_TimeScale(void)
{
    return qvr::timescale::scaleClock.scale;
}

extern "C" float VR_SndRate(void)
{
    return qvr::vr_timescale_sound.value != 0.f ? qvr::timescale::scaleClock.scale : 1.f;
}
