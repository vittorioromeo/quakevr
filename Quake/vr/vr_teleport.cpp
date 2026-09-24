// vr_teleport.cpp -- see vr_teleport.hpp. Ported from the old engine's VR_DoTeleportation.
//
// While +teleport is held, a small box is swept from the player along the off hand's aim, up
// to vr_teleport_range units; the spot is valid when it lands on a floor or a gentle slope
// (normal z 0.75 .. 1), and is marked in blue if valid, red otherwise. Letting
// go on a valid spot sends it with the move (VRBITS0_TELEPORTING); the server moves the
// player there. The aim is drawn as a line with a dot at the destination (vr_lines). The sweep needs the local server's world, so it only works when hosting.

#include "vr_teleport.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_trace.hpp"

namespace qvr::teleport
{
namespace
{

bool held = false;
bool wasHeld = false;
bool impactValid = false;
glm::vec3 impact{0.f};

void teleportDown_f()
{
    held = true;
}

void teleportUp_f()
{
    held = false;
}

[[nodiscard]] bool trace(const hands::State& s)
{
    const glm::vec3 end = s.pos[HAND_OFF] + hands::forward(s.rot[HAND_OFF]) * vr_teleport_range.value;
    const auto found = worldtrace::move(s.playerOrigin, {-6.f, -6.f, -12.f}, {6.f, 6.f, 12.f}, end, MOVE_NORMAL);
    if(!found)
    {
        return false;
    }
    const trace_t& tr = *found;

    impact = {tr.endpos[0], tr.endpos[1], tr.endpos[2] + 12.f}; // player origin is above the feet

    // Floors and slopes, not walls or ceilings.
    return tr.fraction < 1.f && !tr.allsolid && tr.plane.normal[2] >= 0.75f;
}

void drawAim(const hands::State& s)
{
    const glm::vec4 color = impactValid ? glm::vec4{0.2f, 0.4f, 1.f, 0.7f} : glm::vec4{1.f, 0.f, 0.f, 0.5f};
    const glm::vec4 faded{color.r, color.g, color.b, 0.f};

    const glm::vec3 start = s.pos[HAND_OFF];
    const glm::vec3 feet = impact - glm::vec3{0.f, 0.f, 12.f};
    lines::line(start, glm::mix(start, feet, 0.15f), 0.6f, faded, color);
    lines::line(glm::mix(start, feet, 0.15f), feet, 0.6f, color, color);
    lines::point(feet, 12.f, color);
}

} // namespace

void init()
{
    Cmd_AddCommand("+teleport", teleportDown_f);
    Cmd_AddCommand("-teleport", teleportUp_f);
}

bool update(const hands::State& s, glm::vec3& target)
{
    const bool aiming = held && vr_teleport_enabled.value && s.valid;

    bool release = false;
    if(aiming)
    {
        impactValid = trace(s);
        drawAim(s);
    }
    else if(wasHeld && impactValid)
    {
        release = true;
        target = impact;
        impactValid = false;
    }

    wasHeld = aiming;
    return release;
}

} // namespace qvr::teleport
