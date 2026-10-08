// vr_flick.cpp -- see vr_flick.hpp. Ported from the old engine's flick reload (VR_DoInput's
// doFlickReload and VR_UpdateFlick).
//
// With a super shotgun whose clip is not full, a wrist rotation faster than
// vr_spinreload_x_angular_threshold (rad/s) that swings the barrel towards where the hand's
// "up" pointed while it was still is a flick: the server reloads (QC), and the weapon is drawn
// spinning a full turn around the hand's right axis at vr_spinreload_pitch_speed degrees/s, the
// barrel going up and back towards the player first (the old engine's TurnVector(fwd, up, a)).

#include "vr_flick.hpp"
#include "vr_client.hpp"
#include "vr_engine.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"
#include "vr_twohand.hpp"

#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/Sin.hpp"


namespace qvr::flick
{
namespace
{

constexpr int widSuperShotgun = 5; // QC WID_SUPER_SHOTGUN

bool current[2]{false, false};
// cl.time the super shotgun that breaks open was last flicked (its bit held flickHold seconds: one frame's bit was lost
// among the moves the server reads at its own tick, a flick seen late in its swing doing nothing), -1 none.
double flickedAt[2]{-1.0, -1.0};
constexpr double flickHold = 0.12;
float spinLeft[2]{0.f, 0.f};                            // degrees of the visual spin still to go
glm::vec3 restUp[2]{glm::vec3{0.f, 0.f, 1.f}, glm::vec3{0.f, 0.f, 1.f}}; // hand's up while still
double lastTime = -1.0;     // cl.time of the last detection
double lastSpinTime = -1.0; // vr_gametime of the last spin step: every rendered frame, smoothly

constexpr int weaponFlagSsgOpen = 32; // QC's QVR_WPNFLAG_SSG_OPEN

// Immersive reloading's super shotgun (phase 2b, QC vr_reload.qc): the flick breaks it open (no reload, no spin: it is
// drawn open), and, open, snaps it shut (vr_reload_ssg_close_flick). The pry (Pry) does the same with both hands on it.
[[nodiscard]] bool breaksOpen()
{
    return cl.stats[protocol::STAT_QVR_RELOADMODE] == 3 && vr_reload_ssg_break.value != 0.f;
}

[[nodiscard]] bool canFlick(int hand)
{
    using namespace protocol;
    const bool main = hand == HAND_MAIN;
    const int weapon = cl.stats[main ? STAT_QVR_WEAPON : STAT_QVR_WEAPON2];
    const int clip = cl.stats[main ? STAT_QVR_WEAPONCLIP : STAT_QVR_WEAPONCLIP2];
    const int clipSize = cl.stats[main ? STAT_QVR_WEAPONCLIPSIZE : STAT_QVR_WEAPONCLIPSIZE2];
    if(weapon != widSuperShotgun || !twohand::flickAllowed(hand))
    {
        return false;
    }
    if(breaksOpen())
    {
        // (Open by Flick, Close by Flick: each its own switch. Any shells in it are thrown out as it opens.)
        const bool open = cl.stats[main ? STAT_QVR_WEAPONFLAGS : STAT_QVR_WEAPONFLAGS2] & weaponFlagSsgOpen;
        return (open ? vr_reload_ssg_close_flick.value : vr_reload_ssg_open_flick.value) != 0.f;
    }
    return clip != clipSize;
}

// How fast (radians a second) the wrist must turn for a flick of the weapon in `hand`: the super shotgun that breaks open
// its own speeds to flick it open and shut (vr_reload_ssg_flick_open_speed, _close_speed, degrees a second; the author:
// even small flicks opened and shut it); else the classic flick reload's (vr_spinreload_x_angular_threshold).
[[nodiscard]] float flickSpeed(int hand)
{
    using namespace protocol;
    const bool main = hand == HAND_MAIN;
    if(!breaksOpen() || cl.stats[main ? STAT_QVR_WEAPON : STAT_QVR_WEAPON2] != widSuperShotgun)
    {
        return vr_spinreload_x_angular_threshold.value;
    }
    const bool open = cl.stats[main ? STAT_QVR_WEAPONFLAGS : STAT_QVR_WEAPONFLAGS2] & weaponFlagSsgOpen;
    return glm::radians(open ? vr_reload_ssg_flick_close_speed.value : vr_reload_ssg_flick_open_speed.value);
}

// Under how fast (radians a second) the wrist counts as still, its up then what a flick swings the barrel towards: the
// classic flick reload's 1.5; the super shotgun that breaks open vr_reload_ssg_flick_rest (degrees a second), at most 3/4
// of its flick's speed. The author's note of 2026-10-08: after loading, a flick shut it only after a significant wait: the
// gun hand still moving as the pair went in (over 86 deg/s, the old 1.5) kept its up from before it moved, so the flick had
// to swing past that before it counted (the hand had to pause first).
[[nodiscard]] float restSpeed(int hand)
{
    using namespace protocol;
    const bool main = hand == HAND_MAIN;
    if(!breaksOpen() || cl.stats[main ? STAT_QVR_WEAPON : STAT_QVR_WEAPON2] != widSuperShotgun)
    {
        return 1.5f;
    }
    return za::fmin(glm::radians(za::fmax(vr_reload_ssg_flick_rest.value, 0.f)), 0.75f * flickSpeed(hand));
}

// The pry (vr_reload_ssg_pry): both hands on the super shotgun (the other hand's two-handed grip taken, its grip still
// held), the angle of the front hand below the back hand's aim (the controller's calibrated aim, not the two-handed
// aim, which follows the hands' line; the hands where they are tracked, before the grip holds them) going
// vr_reload_ssg_pry_angle up from its least since they took hold, or since the gun last opened or closed, moving at
// vr_reload_ssg_pry_speed degrees a second or more within the last quarter second: the front hand pushing the barrels
// down against the back hand (or the stock lifted) breaks it open; open, as far the other way from its most (the front
// hand lifting the barrels back) snaps it shut (vr_reload_ssg_close_pry). A steady two-handed aim never does: the hands
// and the aim move together. The lift has its own angle, speed and hold (vr_reload_ssg_lift_*). Sent as the OTHER
// hand's flick bit for a moment (pried; its own is the flick): the server toggles it on the edge.
struct Pry
{
    bool armed{false};
    float base{0.f}; // the angle as both took hold
    float lo{0.f};   // the least and the most of it since then (or since the last toggle)
    float hi{0.f};
    float prev{0.f};
    double downAt{-1.0}; // cl.time it last went down fast (the barrels pushed down), and up
    double upAt{-1.0};
    double sentAt{-1.0}; // cl.time the last toggle was sent (the bit held a moment; then a pause)
    double liftSince{-1.0}; // cl.time the barrels were first lifted far and fast enough (vr_reload_ssg_lift_hold)
};
Pry pries[2];
double prevTime = -1.0; // cl.time of the detection before this one

[[nodiscard]] float pryAngle(const hands::State& s, int hand)
{
    const glm::vec3 d = s.unresolvedPos[1 - hand] - s.unresolvedPos[hand];
    const float len = glm::length(d);
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.aimRot[hand], fwd, right, up);
    return len > 1e-3f ? glm::degrees(std::atan2(-glm::dot(d, up) / len, glm::dot(d, fwd) / len)) : 0.f;
}

void updatePry(const hands::State& s, int hand, float dt)
{
    using namespace protocol;
    Pry& p = pries[hand];
    const bool main = hand == HAND_MAIN;
    const int other = 1 - hand;
    const bool ssg = cl.stats[main ? STAT_QVR_WEAPON : STAT_QVR_WEAPON2] == widSuperShotgun && breaksOpen() &&
                     (vr_reload_ssg_pry.value != 0.f || vr_reload_ssg_close_pry.value != 0.f);
    if(!ssg || !client::grabbing(other) || (!p.armed && !twohand::helping(other)))
    {
        p.armed = false;
        return;
    }
    const float a = pryAngle(s, hand);
    if(!p.armed)
    {
        p = Pry{};
        p.armed = true;
        p.base = a;
        if(vr_reload_debug.value && developer.value)
        {
            Con_Printf("ssg: both hands on it (%s hand's gun), the front hand %.1f deg below its aim\n", main ? "main" : "off",
                static_cast<double>(a));
        }
        return;
    }
    const float dev = a - p.base;
    const float rate = dt > 0.f ? (dev - p.prev) / dt : 0.f;
    p.prev = dev;
    if(rate >= vr_reload_ssg_pry_speed.value)
    {
        p.downAt = cl.time;
    }
    if(rate <= -vr_reload_ssg_lift_speed.value)
    {
        p.upAt = cl.time;
    }
    p.lo = za::fmin(p.lo, dev);
    p.hi = za::fmax(p.hi, dev);
    const float need = vr_reload_ssg_pry_angle.value;
    if(vr_reload_debug.value >= 2 && developer.value)
    {
        Con_Printf("ssg: the pry %.1f deg (least %.1f, most %.1f; %.0f deg/s)\n", static_cast<double>(dev),
            static_cast<double>(p.lo), static_cast<double>(p.hi), static_cast<double>(rate));
    }
    if(p.sentAt >= 0.0 && cl.time < p.sentAt + 0.4)
    {
        return;
    }
    const bool open = cl.stats[main ? STAT_QVR_WEAPONFLAGS : STAT_QVR_WEAPONFLAGS2] & weaponFlagSsgOpen;
    const bool pry = !open && vr_reload_ssg_pry.value != 0.f && dev - p.lo >= need && p.downAt >= 0.0 &&
                     cl.time < p.downAt + 0.25;
    // The lift (the author: it shut by accident): its own angle and speed, and held that far for vr_reload_ssg_lift_hold.
    const float liftNeed = vr_reload_ssg_lift_angle.value;
    if(!open || vr_reload_ssg_close_pry.value == 0.f || p.hi - dev < liftNeed)
    {
        p.liftSince = -1.0;
    }
    else if(p.liftSince < 0.0 && p.upAt >= 0.0 && cl.time < p.upAt + 0.25)
    {
        p.liftSince = cl.time;
    }
    const bool lift = p.liftSince >= 0.0 && cl.time - p.liftSince >= za::fmax(vr_reload_ssg_lift_hold.value, 0.f);
    if(pry || lift)
    {
        p.sentAt = cl.time;
        if(developer.value)
        {
            Con_Printf("ssg: %s (%s hand's gun, %.0f deg)\n", pry ? "pried open" : "the barrels lifted shut",
                main ? "main" : "off", static_cast<double>(pry ? dev - p.lo : p.hi - dev));
        }
        p.lo = p.hi = dev;
        p.liftSince = -1.0;
    }
}

} // namespace

bool pried(int hand)
{
    const Pry& p = pries[hand];
    return p.sentAt >= 0.0 && cl.time >= p.sentAt && cl.time < p.sentAt + 0.12;
}

void update(hands::State& s)
{
    const bool newFrame = cl.time != lastTime;
    if(newFrame)
    {
        const float dt = prevTime >= 0.0 ? static_cast<float>(CLAMP(0.0, cl.time - lastTime, 0.1)) : 0.f;
        prevTime = cl.time;
        for(int h = 0; h < HAND_COUNT; h++)
        {
            updatePry(s, h, dt);
        }
    }
    lastTime = cl.time;
    const float spinDt = lastSpinTime >= 0.0 ? static_cast<float>(CLAMP(0.0, vr_gametime - lastSpinTime, 0.1)) : 0.f;
    lastSpinTime = vr_gametime;

    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 fwd, right, up;
        hands::angleVectors(s.rot[h], fwd, right, up);

        if(newFrame)
        {
            const bool before = current[h];
            current[h] = false;

            // The hand's up while it is still (restSpeed), whether it may flick now or not: what a flick swings the barrel
            // towards.
            const float speed = glm::length(s.angVel[h]);
            if(speed < restSpeed(h))
            {
                restUp[h] = up;
            }
            if(canFlick(h))
            {
                current[h] = speed >= flickSpeed(h) && glm::dot(fwd, restUp[h]) > 0.6f;
                if(vr_reload_debug.value >= 2.f && speed >= restSpeed(h))
                {
                    Con_Printf("flick: %s hand turning %.1f rad/s (needs %.1f), the barrel %.2f towards its up at rest (needs 0.6)\n",
                        h == HAND_MAIN ? "main" : "off", speed, flickSpeed(h), glm::dot(fwd, restUp[h]));
                }
                if(current[h] && breaksOpen())
                {
                    flickedAt[h] = cl.time;
                }
                if(current[h] && !before && spinLeft[h] <= 0.f)
                {
                    Con_DPrintf("flick reload (%s hand, %.1f rad/s)\n", h == HAND_MAIN ? "main" : "off", speed);
                    if(!breaksOpen())
                    {
                        spin(h);
                    }
                }
            }
        }
        spinLeft[h] = za::fmax(spinLeft[h] - spinDt * vr_spinreload_pitch_speed.value, 0.f);

        if(spinLeft[h] <= 0.f)
        {
            s.visualRot[h] = s.rot[h];
            continue;
        }

        // Turn the barrel up and back around the hand's right axis.
        const float a = glm::radians(360.f - spinLeft[h]);
        const glm::vec3 spunFwd = fwd * za::cos(a) + up * za::sin(a);
        const glm::vec3 spunUp = up * za::cos(a) - fwd * za::sin(a);
        s.visualRot[h] = hands::anglesFromVectors(spunFwd, spunUp);
    }
}

bool flicking(int hand)
{
    return current[hand] || (flickedAt[hand] >= 0.0 && cl.time >= flickedAt[hand] && cl.time < flickedAt[hand] + flickHold);
}

bool allowed(int hand)
{
    return twohand::flickAllowed(hand);
}

void spin(int hand)
{
    spinLeft[hand] = 360.f;
}

float spinAngle(int hand)
{
    return spinLeft[hand] > 0.f ? 360.f - spinLeft[hand] : -1.f;
}

void reset()
{
    for(int h = 0; h < 2; h++)
    {
        current[h] = false;
        spinLeft[h] = 0.f;
        pries[h] = Pry{};
        flickedAt[h] = -1.0;
    }
    prevTime = -1.0;
    lastTime = -1.0;
    lastSpinTime = -1.0;
}

} // namespace qvr::flick
