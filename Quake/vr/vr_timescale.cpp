// vr_timescale.cpp -- see vr_timescale.hpp.

#include "vr_timescale.hpp"
#include "vr_backend.hpp"
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"

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

// The time scale in effect, eased towards the one asked for; whether the player runs in its own time (Sandevistan);
// and how far vr_gametime has fallen behind realtime.
struct Clock
{
    float scale = 1.f;
    bool sandevistan = false;
    double behind = 0.0;
};
Clock scaleClock;

// A hand as the slowed world has it, while it follows its controller: its place from the head (tracking space, metres)
// and its turn.
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

// The server's entities run in the player's own time (VR_PhysicsEntityBegin/End): the frame each was given, its
// timers before it, the player's clock ahead of the server's, and the edicts free before the player's frame (what it
// spawned in it is the player's too).
constexpr int timerCount = 5; // nextthink and the attack_finished fields (vr_fields.inc)
struct PlayerTime
{
    double worldFrame = 0.0; // the world's frame while an entity runs in the player's time (0: none)
    float before[timerCount]{};
    double offset = 0.0;
    za::Vector<za::U8> wasFree;
    int edicts = 0;
    bool spawns = false; // what the player's frame spawned is looked for (its missiles in its time)
};
PlayerTime playerTime;

// The scale asked for: vr_timescale's, or bullet time's if slower; only for a local single-player game (a remote
// server keeps its own time).
[[nodiscard]] float wanted()
{
    if(!sv.active || svs.maxclients != 1)
    {
        return 1.f;
    }
    float s = 1.f;
    if(vr_timescale.value > 0.f && vr_timescale.value != 1.f)
    {
        s = za::clamp(vr_timescale.value, minScale, 4.f);
    }
    const float bullet = bullettime::scale();
    return bullet < s ? za::max(bullet, minScale) : s;
}

// How many times faster than the world the player's own movement runs (Sandevistan, vr_timescale_move_realtime).
[[nodiscard]] double moveSpeedup()
{
    const float s = scaleClock.scale;
    if(s == 1.f || svs.maxclients != 1 || !(scaleClock.sandevistan || vr_timescale_move_realtime.value != 0.f))
    {
        return 1.0;
    }
    return 1.0 / static_cast<double>(s);
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

// The server's VM for a console command, and back.
struct VmScope
{
    qcvm_t* const old = qcvm;
    VmScope()
    {
        if(old != &sv.qcvm)
        {
            if(old)
            {
                PR_SwitchQCVM(nullptr);
            }
            PR_SwitchQCVM(&sv.qcvm);
        }
    }
    ~VmScope()
    {
        if(old != &sv.qcvm)
        {
            PR_SwitchQCVM(nullptr);
            if(old)
            {
                PR_SwitchQCVM(old);
            }
        }
    }
    VmScope(const VmScope&) = delete;
    VmScope& operator=(const VmScope&) = delete;
};

// vr_slowmo_probe [classname | number]: one line of the clocks (the server's, realtime, vr_gametime, the client's,
// the player's own) and the player's origin and velocity, the main hand's speed and lag, bullet time's meter; with a
// classname, the newest such entity's origin, velocity and nextthink (or that entity's). Slow motion's tests compare
// these at the same game time at different scales (ROUND21.md, "Slow motion").
void probe_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_slowmo_probe: no local game\n");
        return;
    }
    const VmScope vm;
    const edict_t* const pl = sv_player;
    Con_Printf("probe scale %.3f%s sv %.4f real %.4f game %.4f cl %.4f own +%.4f player %.2f %.2f %.2f vel %.1f %.1f %.1f",
               scaleClock.scale, scaleClock.sandevistan ? " sandevistan" : "", qcvm->time, realtime, vr_gametime,
               cl.time, playerTime.offset, pl ? pl->v.origin[0] : 0.f, pl ? pl->v.origin[1] : 0.f,
               pl ? pl->v.origin[2] : 0.f, pl ? pl->v.velocity[0] : 0.f, pl ? pl->v.velocity[1] : 0.f,
               pl ? pl->v.velocity[2] : 0.f);
    {
        const hands::State& hs = hands::current();
        const bullettime::Meter m = bullettime::meter();
        Con_Printf(" | main hand %.2f m/s, %.3f m behind | bullet time %s %.2f | ammo %d %d %d %d clip %d", glm::length(hs.vel[HAND_MAIN]),
                   slowHands.lag[HAND_MAIN], m.active ? "on" : m.cooling ? "cooling" : "off", m.level, cl.stats[STAT_SHELLS], cl.stats[STAT_NAILS], cl.stats[STAT_ROCKETS], cl.stats[STAT_CELLS], cl.stats[protocol::STAT_QVR_WEAPONCLIP]);
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
            Con_Printf(" | %s %d next %.4f at %.2f %.2f %.2f vel %.1f %.1f %.1f", Cmd_Argv(1), NUM_FOR_EDICT(found),
                       found->v.nextthink, found->v.origin[0], found->v.origin[1], found->v.origin[2],
                       found->v.velocity[0], found->v.velocity[1], found->v.velocity[2]);
        }
        else
        {
            Con_Printf(" | no %s", Cmd_Argv(1));
        }
    }
    Con_Printf("\n");
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

// The player's timers (nextthink, then the attack_finished fields), by pointer; null for one the progs lack.
void playerTimers(edict_t* ent, float* out[timerCount])
{
    const progs::FieldOffsets& f = progs::fields();
    const int offsets[timerCount - 1] = {f.attack_finished, f.offhand_attack_finished, f.mainhand_melee_attack_finished,
                                         f.offhand_melee_attack_finished};
    out[0] = &ent->v.nextthink;
    for(int i = 1; i < timerCount; i++)
    {
        out[i] = offsets[i - 1] >= 0 ? progs::fieldPtr(ent, offsets[i - 1]) : nullptr;
    }
}

// A timer set in the player's time (`time` + its seconds): those seconds shortened to the world's.
void intoWorldTime(float& timer, float before, double time, float s)
{
    if(timer != before && timer > static_cast<float>(time))
    {
        timer = static_cast<float>(time + (static_cast<double>(timer) - time) * static_cast<double>(s));
    }
}

[[nodiscard]] bool missileType(const edict_t* ent)
{
    const int m = static_cast<int>(ent->v.movetype);
    return m == MOVETYPE_TOSS || m == MOVETYPE_BOUNCE || m == MOVETYPE_FLY || m == MOVETYPE_FLYMISSILE ||
           m == MOVETYPE_GIB;
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_slowmo", slowmo_f);
    Cmd_AddCommand("vr_slowmo_probe", probe_f);
    bullettime::init();
}

float current()
{
    return scaleClock.scale;
}

float handScale()
{
    return scaleClock.sandevistan ? 1.f : scaleClock.scale;
}

float handLag(int hand)
{
    return hand >= 0 && hand < HAND_COUNT ? slowHands.lag[hand] : 0.f;
}

bool sandevistan()
{
    return scaleClock.sandevistan;
}

float turnSpeedup()
{
    const float s = scaleClock.scale;
    return s != 1.f && (scaleClock.sandevistan || vr_timescale_turn_realtime.value != 0.f) ? 1.f / s : 1.f;
}

void filterHands(TrackingState& t)
{
    const float s = scaleClock.scale;
    const bool slowed = s != 1.f && !scaleClock.sandevistan; // the hands slowed with the world

    // The runtime clock, slowed with the world: what the throws' and spins' velocities are measured on.
    if(t.time >= 0.0)
    {
        if(slowHands.lastRuntime >= 0.0 && t.time > slowHands.lastRuntime && slowed)
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
    if(!inGame || scaleClock.sandevistan || (s == 1.f && !following))
    {
        for(int h = 0; h < HAND_COUNT; h++)
        {
            slowHands.hand[h].on = false;
            slowHands.lag[h] = 0.f;
        }
        return;
    }

    // Each hand follows from the head (never slowed): walking in the room, ducking, a dodge carry the hands along as
    // they are; only their own moves from the head are slowed (and QC's melee measures them from the head).
    const glm::vec3 head = t.head.valid ? t.head.position : glm::vec3{0.f};
    const glm::vec3 headVel = t.head.valid && t.head.velocityValid ? t.head.linearVelocity : glm::vec3{0.f};
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
        const glm::vec3 fromHead = p.position - head;
        if(!f.on)
        {
            f.pos = fromHead;
            f.rot = p.orientation;
            f.on = true;
        }

        // Towards the controller, as fast as the slowed world lets it.
        const glm::vec3 d = fromHead - f.pos;
        const float len = glm::length(d);
        const bool limited = len > maxStep;
        const glm::vec3 step = limited ? d * (maxStep / len) : d;
        f.pos += step;

        glm::vec3 axis;
        const float angle = turnBetween(f.rot, p.orientation, axis);
        const bool turnLimited = angle > maxTurn;
        f.rot = turnLimited ? glm::normalize(glm::slerp(f.rot, p.orientation, maxTurn / angle)) : p.orientation;

        // Its velocities in the game's time: the controller's, sped up as the world is slowed (a hand moved slowly
        // is a normal blow in the slowed world), or the head's and the most the follower moves.
        if(p.velocityValid)
        {
            const float toGame = 1.f / s;
            const glm::vec3 moved = headVel * toGame + (dt > 0.f ? step / dt : glm::vec3{0.f});
            p.linearVelocity = limited ? moved : p.linearVelocity * toGame;
            p.gripVelocity = limited ? moved : p.gripVelocity * toGame;
            p.angularVelocity = turnLimited ? axis * (dt > 0.f ? maxTurn / dt : 0.f) : p.angularVelocity * toGame;
        }
        const glm::vec3 drawn = head + f.pos;
        slowHands.lag[h] = glm::length(p.position - drawn);
        p.position = drawn;
        p.orientation = f.rot;

        // Back at a time scale of 1 and caught up: the controller's own pose from the next frame on.
        if(s == 1.f && !limited && !turnLimited)
        {
            f.on = false;
        }
    }
}

} // namespace qvr::timescale

using qvr::timescale::playerTime;
using qvr::timescale::scaleClock;

// Host_AdvanceTime, after realtime advanced by dt: the time scale eased towards the one asked for (over
// vr_timescale_ramp seconds for the whole way from 1), bullet time's meter, and vr_gametime (the player's body's
// clock: slowed with the world, but not in Sandevistan).
extern "C" void VR_AdvanceTime(double dt)
{
    qvr::bullettime::advance(dt);
    const float target = qvr::timescale::wanted();
    if(scaleClock.scale != target)
    {
        const float ramp = qvr::vr_timescale_ramp.value;
        const float most = ramp > 0.f ? static_cast<float>(dt) / ramp : 1.f;
        const float gap = target - scaleClock.scale;
        scaleClock.scale = za::fabs(gap) <= most ? target : scaleClock.scale + (gap > 0.f ? most : -most);
    }
    scaleClock.sandevistan = scaleClock.scale < 1.f && svs.maxclients == 1 &&
                             (qvr::vr_sandevistan.value != 0.f || qvr::bullettime::sandevistan());
    if(scaleClock.scale != 1.f && !scaleClock.sandevistan)
    {
        scaleClock.behind += dt * (1.0 - static_cast<double>(scaleClock.scale));
    }
    vr_gametime = scaleClock.behind != 0.0 ? realtime - scaleClock.behind : realtime;
    if(!sv.active)
    {
        playerTime.offset = 0.0;
    }
}

extern "C" double VR_TimeScale(void)
{
    return scaleClock.scale;
}

extern "C" float VR_SndRate(void)
{
    return qvr::vr_timescale_sound.value != 0.f ? scaleClock.scale : 1.f;
}

extern "C" double VR_PlayerMoveSpeedup(void)
{
    return qvr::timescale::moveSpeedup();
}

// Host_ServerFrame, round SV_RunClients: the player's clock for QC (vr_player_time_offset); the player's moves (Quake's acceleration and friction) in its own time.
extern "C" double VR_PlayerRunBegin(void)
{
    // The player's clock for QC's melee (0 until a Sandevistan; then as far ahead as it got, for good).
    const qvr::progs::Bindings& b = qvr::progs::bindings();
    if(b.playerTimeOffset)
    {
        *b.playerTimeOffset = static_cast<float>(playerTime.offset);
    }
    const double k = qvr::timescale::moveSpeedup();
    if(k == 1.0)
    {
        return 0.0;
    }
    const double world = host_frametime;
    host_frametime = world * k;
    pr_global_struct->frametime = static_cast<float>(host_frametime);
    return world;
}

extern "C" void VR_PlayerRunEnd(double world)
{
    if(world > 0.0)
    {
        host_frametime = world;
        pr_global_struct->frametime = static_cast<float>(world);
    }
}

// SV_Physics, round each entity's: the player (Sandevistan, vr_timescale_move_realtime) and its missiles
// (Sandevistan, vr_sandevistan_missiles) moved and thinking in the player's own time: their frame the player's
// (host_frametime and QC's frametime: the world's over the scale), and the timers they set in it (nextthink, the
// player's attack_finished fields; the nextthink of what the player spawned) shortened to the world's time. Returns
// the world's frame to put back (0: the entity runs in the world's time).
extern "C" double VR_PhysicsEntityBegin(edict_t* ent, int num)
{
    using namespace qvr::timescale;
    const double k = moveSpeedup();
    if(k == 1.0)
    {
        return 0.0;
    }
    const bool player = num == 1;
    if(!player && !(scaleClock.sandevistan && qvr::vr_sandevistan_missiles.value != 0.f && missileType(ent) &&
                    ent->v.owner == EDICT_TO_PROG(EDICT_NUM(1))))
    {
        return 0.0;
    }
    const double world = host_frametime;
    host_frametime = world * k;
    pr_global_struct->frametime = static_cast<float>(host_frametime);
    playerTime.worldFrame = world;
    playerTime.before[0] = ent->v.nextthink;
    if(player)
    {
        float* timers[timerCount];
        playerTimers(ent, timers);
        for(int i = 1; i < timerCount; i++)
        {
            playerTime.before[i] = timers[i] ? *timers[i] : 0.f;
        }
        playerTime.spawns = scaleClock.sandevistan && qvr::vr_sandevistan_missiles.value != 0.f;
        if(playerTime.spawns)
        {
            playerTime.edicts = qcvm->num_edicts;
            playerTime.wasFree.resize(static_cast<za::SizeT>(qcvm->num_edicts));
            for(int i = 0; i < qcvm->num_edicts; i++)
            {
                playerTime.wasFree[static_cast<za::SizeT>(i)] = EDICT_NUM(i)->free ? 1 : 0;
            }
        }
    }
    return world;
}

extern "C" void VR_PhysicsEntityEnd(edict_t* ent, int num, double world)
{
    using namespace qvr::timescale;
    host_frametime = world;
    pr_global_struct->frametime = static_cast<float>(world);
    playerTime.worldFrame = 0.0;
    if(ent->free)
    {
        return;
    }
    const double time = qcvm->time;
    const float s = scaleClock.scale;
    intoWorldTime(ent->v.nextthink, playerTime.before[0], time, s);
    if(num != 1)
    {
        return;
    }
    float* timers[timerCount];
    playerTimers(ent, timers);
    for(int i = 1; i < timerCount; i++)
    {
        if(timers[i])
        {
            intoWorldTime(*timers[i], playerTime.before[i], time, s);
        }
    }
    // What the player spawned in its frame (a rocket, a grenade, a thrown weapon): its first think in the player's time.
    if(playerTime.spawns)
    {
        const int owner = EDICT_TO_PROG(ent);
        for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
        {
            edict_t* const e = EDICT_NUM(i);
            const bool fresh = i >= playerTime.edicts || playerTime.wasFree[static_cast<za::SizeT>(i)] != 0;
            if(fresh && !e->free && e->v.owner == owner && e->v.nextthink > static_cast<float>(time))
            {
                e->v.nextthink = static_cast<float>(time + (static_cast<double>(e->v.nextthink) - time) * s);
            }
        }
    }
    // The player's clock: ahead by what its frame was longer than the world's (Sandevistan only: the melee's).
    if(scaleClock.sandevistan)
    {
        playerTime.offset += world * (1.0 / static_cast<double>(s) - 1.0);
    }
}

extern "C" double VR_ThinkFrame(double frametime)
{
    return playerTime.worldFrame > 0.0 ? playerTime.worldFrame : frametime;
}
