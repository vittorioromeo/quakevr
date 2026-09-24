// vr_flick.cpp -- see vr_flick.hpp. Ported from the old engine's flick reload (VR_DoInput's
// doFlickReload and VR_UpdateFlick).
//
// With a super shotgun whose clip is not full, a wrist rotation faster than
// vr_spinreload_x_angular_threshold (rad/s) that swings the barrel towards where the hand's
// "up" pointed while it was still is a flick: the server reloads (QC), and the weapon is drawn
// spinning a full turn around the hand's right axis at vr_spinreload_pitch_speed degrees/s.

#include "vr_flick.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"

#include <cmath>

namespace qvr::flick
{
namespace
{

constexpr int widSuperShotgun = 5; // QC WID_SUPER_SHOTGUN

bool current[2]{false, false};
float spinLeft[2]{0.f, 0.f};                            // degrees of the visual spin still to go
glm::vec3 restUp[2]{glm::vec3{0.f, 0.f, 1.f}, glm::vec3{0.f, 0.f, 1.f}}; // hand's up while still
double lastTime = -1.0;

[[nodiscard]] bool canFlick(int hand)
{
    using namespace protocol;
    const bool main = hand == HAND_MAIN;
    const int weapon = cl.stats[main ? STAT_QVR_WEAPON : STAT_QVR_WEAPON2];
    const int clip = cl.stats[main ? STAT_QVR_WEAPONCLIP : STAT_QVR_WEAPONCLIP2];
    const int clipSize = cl.stats[main ? STAT_QVR_WEAPONCLIPSIZE : STAT_QVR_WEAPONCLIPSIZE2];
    return weapon == widSuperShotgun && clip != clipSize;
}

} // namespace

void update(hands::State& s)
{
    const float dt = lastTime >= 0.0 ? static_cast<float>(std::fmax(cl.time - lastTime, 0.0)) : 0.f;
    const bool newFrame = cl.time != lastTime;
    lastTime = cl.time;

    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 fwd, right, up;
        hands::angleVectors(s.rot[h], fwd, right, up);

        if(newFrame)
        {
            const bool before = current[h];
            current[h] = false;

            if(canFlick(h))
            {
                const float speed = glm::length(s.angVel[h]);
                if(speed < 1.5f)
                {
                    restUp[h] = up;
                }

                current[h] = speed >= vr_spinreload_x_angular_threshold.value && glm::dot(fwd, restUp[h]) > 0.6f;
                if(current[h] && !before && spinLeft[h] <= 0.f)
                {
                    Con_DPrintf("flick reload (%s hand, %.1f rad/s)\n", h == HAND_MAIN ? "main" : "off", speed);
                    spin(h);
                }
            }

            spinLeft[h] = std::fmax(spinLeft[h] - dt * vr_spinreload_pitch_speed.value, 0.f);
        }

        if(spinLeft[h] <= 0.f)
        {
            s.visualRot[h] = s.rot[h];
            continue;
        }

        // Pitch the weapon forward around the hand's right axis.
        const float a = glm::radians(360.f - spinLeft[h]);
        const glm::vec3 spunFwd = fwd * std::cos(a) - up * std::sin(a);
        const glm::vec3 spunUp = up * std::cos(a) + fwd * std::sin(a);
        s.visualRot[h] = hands::anglesFromVectors(spunFwd, spunUp);
    }
}

bool flicking(int hand)
{
    return current[hand];
}

void spin(int hand)
{
    spinLeft[hand] = 360.f;
}

void reset()
{
    for(int h = 0; h < 2; h++)
    {
        current[h] = false;
        spinLeft[h] = 0.f;
    }
    lastTime = -1.0;
}

} // namespace qvr::flick
