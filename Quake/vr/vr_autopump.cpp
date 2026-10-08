// vr_autopump.cpp -- see vr_autopump.hpp.
//
// A stroke over vr_autopump_time T: back over its first 35% (fast, easing into the back: the gas has driven it, the
// buffer stops it), held there 10% (the shell leaves), forward over the rest (the spring: from rest, faster and faster,
// slamming home). The clacks: the first (the action unlocking, the slide starting back) as the stroke starts, the
// second (the slide slamming home) ahead of its end by its lead, so its loudest moment is the stroke's end.

#include "vr_autopump.hpp"
#include "vr_engine.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::autopump
{
namespace
{

constexpr float backPart = 0.35f; // of the stroke: going back,
constexpr float holdPart = 0.10f; // held at the back; the rest coming forward
constexpr float maxTravel = 3.2f; // model units (the fore-end's back then meets the actuator housings)
constexpr double shotWindow = 0.1; // a shell's eject message within this of the stroke's shot is that shot's

constexpr const char* backSoundName = "vr/autopump_back.wav";
constexpr const char* homeSoundName = "vr/autopump_home.wav";
constexpr float homeSoundLead = 0.053f; // seconds from the home clack's start to its loudest moment

struct Stroke
{
    double shot{-1.0};  // game time of the shot (-1: none)
    double start{-1.0}; // game time it starts, vr_autopump_delay after the shot (-1: none)
    float time{0.3f};   // its length, as vr_autopump_time was then
    float travel{2.5f}; // and its travel
    bool backSounded{false};
    bool rearLogged{false};
    bool homeSounded{false};
    bool endLogged{false};
};

Stroke strokes[2];
sfx_t* backSound = nullptr;
sfx_t* homeSound = nullptr;

[[nodiscard]] bool on()
{
    return vr_autopump.value != 0.f;
}

[[nodiscard]] float strokeTime()
{
    return za::clamp(vr_autopump_time.value, 0.1f, 0.48f); // (the shotgun refires after 0.5 s)
}

// The shot, a moment, then the stroke (the author's note, 2026-10-08 13:58: it started with the shot): cut short so that
// the stroke is home before the shotgun can fire again.
[[nodiscard]] float strokeDelay(float time)
{
    return za::clamp(vr_autopump_delay.value, 0.f, za::max(0.f, 0.48f - time));
}

[[nodiscard]] float strokeTravel()
{
    return za::clamp(vr_autopump_travel.value, 0.f, maxTravel);
}

// How far back (0..1) at `u` (0..1) of the stroke.
[[nodiscard]] float position(float u)
{
    if(u <= 0.f || u >= 1.f)
    {
        return 0.f;
    }
    if(u < backPart)
    {
        const float a = 1.f - u / backPart;
        return 1.f - a * a * a; // fast away, easing into the back
    }
    if(u < backPart + holdPart)
    {
        return 1.f;
    }
    const float b = (u - backPart - holdPart) / (1.f - backPart - holdPart);
    return 1.f - b * b; // from rest, faster and faster, home at full speed
}

[[nodiscard]] float progress(const Stroke& s)
{
    return s.start < 0.0 ? 1.f : static_cast<float>((cl.time - s.start) / s.time);
}

void tick(int hand, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value && vr_autopump_haptics.value > 0.f)
    {
        be->haptic(hand, 0.02f, 160.f, za::clamp(amplitude * vr_autopump_haptics.value, 0.f, 1.f));
    }
}

void sound(sfx_t* sfx, const float (&where)[3])
{
    const float volume = za::clamp(vr_autopump_sound.value, 0.f, 1.f);
    if(sfx && volume > 0.f)
    {
        vec3_t org{where[0], where[1], where[2]};
        S_StartSound(0, 0, sfx, org, volume, 1.5f); // (there, as the shells' tinks: not the player's own channel)
    }
}

} // namespace

void fired(int hand)
{
    if(hand != HAND_OFF && hand != HAND_MAIN)
    {
        return;
    }
    Stroke& s = strokes[hand];
    s = Stroke{};
    if(!on())
    {
        return;
    }
    s.shot = cl.time;
    s.time = strokeTime();
    s.start = cl.time + strokeDelay(s.time);
    s.travel = strokeTravel();
    if(vr_debug_weaponfx.value)
    {
        Con_Printf("autopump hand %d start t %.3f (real %.3f): back at %.3f, home at %.3f, %.2f units, shot at %.3f\n",
            hand, s.start, realtime, s.start + s.time * backPart, s.start + s.time, s.travel, s.shot);
    }
}

float travel(int hand)
{
    if(hand != HAND_OFF && hand != HAND_MAIN)
    {
        return 0.f;
    }
    if(vr_autopump_hold.value >= 0.f)
    {
        return position(za::min(vr_autopump_hold.value, 0.9999f)) * strokeTravel();
    }
    const Stroke& s = strokes[hand];
    return on() && s.start >= 0.0 ? position(progress(s)) * s.travel : 0.f;
}

bool rearTime(int hand, double shot, double& rear, float& backSpeed)
{
    if((hand != HAND_OFF && hand != HAND_MAIN) || !on())
    {
        return false;
    }
    const Stroke& s = strokes[hand];
    if(s.start < 0.0 || s.shot < shot - shotWindow || s.shot > shot + shotWindow)
    {
        return false;
    }
    rear = s.start + s.time * backPart;
    backSpeed = s.travel / (s.time * backPart);
    return true;
}

void frame(const bool (&isShotgun)[2], const float (&where)[2][3])
{
    for(int hand = 0; hand < 2; hand++)
    {
        Stroke& s = strokes[hand];
        if(s.start < 0.0)
        {
            continue;
        }
        if(!on() || cl.time < s.shot || cl.time > s.start + s.time + 1.0)
        {
            s = Stroke{}; // (a time going back: a new map, a demo)
            continue;
        }
        if(cl.time < s.start)
        {
            continue; // (the delay after the shot: vr_autopump_delay)
        }
        const float u = progress(s);
        if(!s.backSounded)
        {
            s.backSounded = true;
            if(isShotgun[hand])
            {
                sound(backSound, where[hand]);
            }
        }
        if(!s.rearLogged && u >= backPart)
        {
            s.rearLogged = true;
            if(isShotgun[hand])
            {
                tick(hand, 0.35f);
            }
            if(vr_debug_weaponfx.value)
            {
                Con_Printf("autopump hand %d back t %.3f (real %.3f; due %.3f), %.2f units\n", hand, cl.time, realtime,
                    s.start + s.time * backPart, position(u) * s.travel);
            }
        }
        if(!s.homeSounded && cl.time >= s.start + s.time - homeSoundLead)
        {
            s.homeSounded = true;
            if(isShotgun[hand])
            {
                sound(homeSound, where[hand]);
            }
        }
        if(!s.endLogged && u >= 1.f)
        {
            s.endLogged = true;
            if(isShotgun[hand])
            {
                tick(hand, 0.5f);
            }
            if(vr_debug_weaponfx.value)
            {
                Con_Printf("autopump hand %d home t %.3f (real %.3f; due %.3f)\n", hand, cl.time, realtime,
                    s.start + s.time);
            }
        }
    }
}

void prepare()
{
    backSound = S_PrecacheSound(backSoundName);
    homeSound = S_PrecacheSound(homeSoundName);
}

void clear()
{
    for(Stroke& s : strokes)
    {
        s = Stroke{};
    }
}

} // namespace qvr::autopump
